#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <bcrypt.h>
#include <winreg.h>

#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "bakkesmod/wrappers/GameWrapper.h"
#include "bakkesmod/wrappers/items/ItemsWrapper.h"
#include "bakkesmod/wrappers/items/ProductWrapper.h"
#include "bakkesmod/wrappers/items/ProductSlotWrapper.h"
#include "bakkesmod/wrappers/items/ProductTemplateWrapper.h"
#include "bakkesmod/wrappers/items/attributes/ProductAttributeWrapper.h"

#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <functional>
#include <memory>
#include <cstdint>
#include <cstring>
#include <algorithm>
#include <unordered_map>

#pragma comment(lib, "BCrypt.lib")
