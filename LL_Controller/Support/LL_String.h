#pragma once
#include <string>
#include <vector>
int LL_SplitString(const std::string& text, const std::string& mark, std::vector<std::string>& out);
bool LL_isInt(const std::string& text);

bool LL_Str2Int(const std::string& text, int& value);
bool LL_Str2Num(const std::string& text, double& value);
