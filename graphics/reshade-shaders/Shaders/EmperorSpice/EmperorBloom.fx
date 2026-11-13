// Emperor Spice - simple bloom for Emperor: Battle for Dune
// Bright parts of the image (explosions, lasers, lights, sunlit sand) get a soft glow.
// MIT License, Copyright (c) 2026 Emperor Spice contributors.

#include "ReShade.fxh"

uniform float BloomThreshold <
	ui_type = "slider"; ui_min = 0.0; ui_max = 1.0; ui_step = 0.01;
	ui_label = "Threshold";
	ui_tooltip = "Only parts brighter than this glow.";
> = 0.70;

uniform float BloomStrength <
	ui_type = "slider"; ui_min = 0.0; ui_max = 2.0; ui_step = 0.01;
	ui_label = "Strength";
> = 0.35;

uniform float BloomSaturation <
	ui_type = "slider"; ui_min = 0.0; ui_max = 2.0; ui_step = 0.01;
	ui_label = "Glow saturation";
> = 1.10;

texture EmperorBloomTexA { Width = BUFFER_WIDTH / 4; Height = BUFFER_HEIGHT / 4; Format = RGBA16F; };
texture EmperorBloomTexB { Width = BUFFER_WIDTH / 4; Height = BUFFER_HEIGHT / 4; Format = RGBA16F; };
sampler EmperorBloomSamplerA { Texture = EmperorBloomTexA; };
sampler EmperorBloomSamplerB { Texture = EmperorBloomTexB; };

static const float kWeights[5] = { 0.2270270270, 0.1945945946, 0.1216216216, 0.0540540541, 0.0162162162 };

float3 BrightPart(float3 c)
{
	float luma = dot(c, float3(0.2126, 0.7152, 0.0722));
	float k = saturate((luma - BloomThreshold) / max(1.0 - BloomThreshold, 0.001));
	float3 grey = luma.xxx;
	return lerp(grey, c, BloomSaturation) * k;
}

// Pass 1: bright parts, downsampled to quarter size with a 4-tap box filter
float4 PS_Extract(float4 pos : SV_Position, float2 uv : TEXCOORD) : SV_Target
{
	float2 o = BUFFER_PIXEL_SIZE * 1.5;
	float3 c = BrightPart(tex2D(ReShade::BackBuffer, uv + float2(-o.x, -o.y)).rgb);
	c += BrightPart(tex2D(ReShade::BackBuffer, uv + float2( o.x, -o.y)).rgb);
	c += BrightPart(tex2D(ReShade::BackBuffer, uv + float2(-o.x,  o.y)).rgb);
	c += BrightPart(tex2D(ReShade::BackBuffer, uv + float2( o.x,  o.y)).rgb);
	return float4(c * 0.25, 1.0);
}

float4 Blur(sampler s, float2 uv, float2 dir)
{
	float2 stp = dir * BUFFER_PIXEL_SIZE * 4.0 * 1.5;
	float3 c = tex2D(s, uv).rgb * kWeights[0];
	[unroll] for (int i = 1; i < 5; i++)
	{
		c += tex2D(s, uv + stp * i).rgb * kWeights[i];
		c += tex2D(s, uv - stp * i).rgb * kWeights[i];
	}
	return float4(c, 1.0);
}

float4 PS_BlurH(float4 pos : SV_Position, float2 uv : TEXCOORD) : SV_Target { return Blur(EmperorBloomSamplerA, uv, float2(1.0, 0.0)); }
float4 PS_BlurV(float4 pos : SV_Position, float2 uv : TEXCOORD) : SV_Target { return Blur(EmperorBloomSamplerB, uv, float2(0.0, 1.0)); }

// Pass 4: add the glow, screen blend so bright areas don't clip harshly
float4 PS_Combine(float4 pos : SV_Position, float2 uv : TEXCOORD) : SV_Target
{
	float3 c = tex2D(ReShade::BackBuffer, uv).rgb;
	float3 glow = tex2D(EmperorBloomSamplerA, uv).rgb * BloomStrength;
	return float4(1.0 - (1.0 - c) * (1.0 - saturate(glow)), 1.0);
}

technique EmperorBloom <
	ui_label = "Emperor Spice: Bloom";
	ui_tooltip = "Soft glow on explosions, lasers and lights.";
>
{
	pass Extract { VertexShader = PostProcessVS; PixelShader = PS_Extract; RenderTarget = EmperorBloomTexA; }
	pass BlurH   { VertexShader = PostProcessVS; PixelShader = PS_BlurH;   RenderTarget = EmperorBloomTexB; }
	pass BlurV   { VertexShader = PostProcessVS; PixelShader = PS_BlurV;   RenderTarget = EmperorBloomTexA; }
	pass Combine { VertexShader = PostProcessVS; PixelShader = PS_Combine; }
}
