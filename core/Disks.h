#pragma once
#include <string>

std::string enumPartitions();
int GetVolumeProtectionStatus(char drive_letter);
const char* VolumeProtectionStatus_tostring(int t);
const wchar_t* VolumeProtectionStatus_tostringzh(int t);
