#pragma once
#include <map>
#include <string>

// isoImages: volume label ("EMPEROR1".."EMPEROR4") -> path of an .iso file to read that disc from.
// Discs without an entry are read from a CD drive / mounted image as before.
// Returns false if reading an ISO image failed.
bool installEmperor(const std::string& installDir, const std::map<std::string, std::wstring>& isoImages = {});
