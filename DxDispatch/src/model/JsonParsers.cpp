#include "pch.h"
#include "JsonParsers.h"
#include "StdSupport.h"
#include "NpyReaderWriter.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include "ImageReaderWriter.h"
#include <cstring>
#ifndef WIN32
#define _stricmp strcasecmp
#endif

using Microsoft::WRL::ComPtr;

std::string RapidJsonToString(const rapidjson::Value& value)
{
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    value.Accept(writer);
    return buffer.GetString();
}

static uint32_t GetSizeInBytes(DML_TENSOR_DATA_TYPE dataType)
{
    switch (dataType)
    {
        case DML_TENSOR_DATA_TYPE_INT8:
        case DML_TENSOR_DATA_TYPE_UINT8:
            return 1;

        case DML_TENSOR_DATA_TYPE_FLOAT16:
        case DML_TENSOR_DATA_TYPE_INT16:
        case DML_TENSOR_DATA_TYPE_UINT16:
            return 2;

        case DML_TENSOR_DATA_TYPE_FLOAT32:
        case DML_TENSOR_DATA_TYPE_INT32:
        case DML_TENSOR_DATA_TYPE_UINT32:
            return 4;
        case DML_TENSOR_DATA_TYPE_FLOAT64:
        case DML_TENSOR_DATA_TYPE_INT64:
        case DML_TENSOR_DATA_TYPE_UINT64:
            return 8;

        default:
            throw std::invalid_argument("Unexpected DML_TENSOR_DATA_TYPE");
    }
}

DXGI_FORMAT ParseDxgiFormat(const rapidjson::Value& value)
{
    if (value.GetType() != rapidjson::Type::kStringType)
    {
        throw std::invalid_argument("DML_OPERATOR_TYPE must be a string.");
    }
    auto valueString = value.GetString();

    if (!strcmp(valueString, "DXGI_FORMAT_UNKNOWN") || !strcmp(valueString, "UNKNOWN")) { return DXGI_FORMAT_UNKNOWN; }
    if (!strcmp(valueString, "DXGI_FORMAT_R32G32B32A32_TYPELESS") || !strcmp(valueString, "R32G32B32A32_TYPELESS")) { return DXGI_FORMAT_R32G32B32A32_TYPELESS; }
    if (!strcmp(valueString, "DXGI_FORMAT_R32G32B32A32_FLOAT") || !strcmp(valueString, "R32G32B32A32_FLOAT")) { return DXGI_FORMAT_R32G32B32A32_FLOAT; }
    if (!strcmp(valueString, "DXGI_FORMAT_R32G32B32A32_UINT") || !strcmp(valueString, "R32G32B32A32_UINT")) { return DXGI_FORMAT_R32G32B32A32_UINT; }
    if (!strcmp(valueString, "DXGI_FORMAT_R32G32B32A32_SINT") || !strcmp(valueString, "R32G32B32A32_SINT")) { return DXGI_FORMAT_R32G32B32A32_SINT; }
    if (!strcmp(valueString, "DXGI_FORMAT_R32G32B32_TYPELESS") || !strcmp(valueString, "R32G32B32_TYPELESS")) { return DXGI_FORMAT_R32G32B32_TYPELESS; }
    if (!strcmp(valueString, "DXGI_FORMAT_R32G32B32_FLOAT") || !strcmp(valueString, "R32G32B32_FLOAT")) { return DXGI_FORMAT_R32G32B32_FLOAT; }
    if (!strcmp(valueString, "DXGI_FORMAT_R32G32B32_UINT") || !strcmp(valueString, "R32G32B32_UINT")) { return DXGI_FORMAT_R32G32B32_UINT; }
    if (!strcmp(valueString, "DXGI_FORMAT_R32G32B32_SINT") || !strcmp(valueString, "R32G32B32_SINT")) { return DXGI_FORMAT_R32G32B32_SINT; }
    if (!strcmp(valueString, "DXGI_FORMAT_R16G16B16A16_TYPELESS") || !strcmp(valueString, "R16G16B16A16_TYPELESS")) { return DXGI_FORMAT_R16G16B16A16_TYPELESS; }
    if (!strcmp(valueString, "DXGI_FORMAT_R16G16B16A16_FLOAT") || !strcmp(valueString, "R16G16B16A16_FLOAT")) { return DXGI_FORMAT_R16G16B16A16_FLOAT; }
    if (!strcmp(valueString, "DXGI_FORMAT_R16G16B16A16_UNORM") || !strcmp(valueString, "R16G16B16A16_UNORM")) { return DXGI_FORMAT_R16G16B16A16_UNORM; }
    if (!strcmp(valueString, "DXGI_FORMAT_R16G16B16A16_UINT") || !strcmp(valueString, "R16G16B16A16_UINT")) { return DXGI_FORMAT_R16G16B16A16_UINT; }
    if (!strcmp(valueString, "DXGI_FORMAT_R16G16B16A16_SNORM") || !strcmp(valueString, "R16G16B16A16_SNORM")) { return DXGI_FORMAT_R16G16B16A16_SNORM; }
    if (!strcmp(valueString, "DXGI_FORMAT_R16G16B16A16_SINT") || !strcmp(valueString, "R16G16B16A16_SINT")) { return DXGI_FORMAT_R16G16B16A16_SINT; }
    if (!strcmp(valueString, "DXGI_FORMAT_R32G32_TYPELESS") || !strcmp(valueString, "R32G32_TYPELESS")) { return DXGI_FORMAT_R32G32_TYPELESS; }
    if (!strcmp(valueString, "DXGI_FORMAT_R32G32_FLOAT") || !strcmp(valueString, "R32G32_FLOAT")) { return DXGI_FORMAT_R32G32_FLOAT; }
    if (!strcmp(valueString, "DXGI_FORMAT_R32G32_UINT") || !strcmp(valueString, "R32G32_UINT")) { return DXGI_FORMAT_R32G32_UINT; }
    if (!strcmp(valueString, "DXGI_FORMAT_R32G32_SINT") || !strcmp(valueString, "R32G32_SINT")) { return DXGI_FORMAT_R32G32_SINT; }
    if (!strcmp(valueString, "DXGI_FORMAT_R32G8X24_TYPELESS") || !strcmp(valueString, "R32G8X24_TYPELESS")) { return DXGI_FORMAT_R32G8X24_TYPELESS; }
    if (!strcmp(valueString, "DXGI_FORMAT_D32_FLOAT_S8X24_UINT") || !strcmp(valueString, "D32_FLOAT_S8X24_UINT")) { return DXGI_FORMAT_D32_FLOAT_S8X24_UINT; }
    if (!strcmp(valueString, "DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS") || !strcmp(valueString, "R32_FLOAT_X8X24_TYPELESS")) { return DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS; }
    if (!strcmp(valueString, "DXGI_FORMAT_X32_TYPELESS_G8X24_UINT") || !strcmp(valueString, "X32_TYPELESS_G8X24_UINT")) { return DXGI_FORMAT_X32_TYPELESS_G8X24_UINT; }
    if (!strcmp(valueString, "DXGI_FORMAT_R10G10B10A2_TYPELESS") || !strcmp(valueString, "R10G10B10A2_TYPELESS")) { return DXGI_FORMAT_R10G10B10A2_TYPELESS; }
    if (!strcmp(valueString, "DXGI_FORMAT_R10G10B10A2_UNORM") || !strcmp(valueString, "R10G10B10A2_UNORM")) { return DXGI_FORMAT_R10G10B10A2_UNORM; }
    if (!strcmp(valueString, "DXGI_FORMAT_R10G10B10A2_UINT") || !strcmp(valueString, "R10G10B10A2_UINT")) { return DXGI_FORMAT_R10G10B10A2_UINT; }
    if (!strcmp(valueString, "DXGI_FORMAT_R11G11B10_FLOAT") || !strcmp(valueString, "R11G11B10_FLOAT")) { return DXGI_FORMAT_R11G11B10_FLOAT; }
    if (!strcmp(valueString, "DXGI_FORMAT_R8G8B8A8_TYPELESS") || !strcmp(valueString, "R8G8B8A8_TYPELESS")) { return DXGI_FORMAT_R8G8B8A8_TYPELESS; }
    if (!strcmp(valueString, "DXGI_FORMAT_R8G8B8A8_UNORM") || !strcmp(valueString, "R8G8B8A8_UNORM")) { return DXGI_FORMAT_R8G8B8A8_UNORM; }
    if (!strcmp(valueString, "DXGI_FORMAT_R8G8B8A8_UNORM_SRGB") || !strcmp(valueString, "R8G8B8A8_UNORM_SRGB")) { return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB; }
    if (!strcmp(valueString, "DXGI_FORMAT_R8G8B8A8_UINT") || !strcmp(valueString, "R8G8B8A8_UINT")) { return DXGI_FORMAT_R8G8B8A8_UINT; }
    if (!strcmp(valueString, "DXGI_FORMAT_R8G8B8A8_SNORM") || !strcmp(valueString, "R8G8B8A8_SNORM")) { return DXGI_FORMAT_R8G8B8A8_SNORM; }
    if (!strcmp(valueString, "DXGI_FORMAT_R8G8B8A8_SINT") || !strcmp(valueString, "R8G8B8A8_SINT")) { return DXGI_FORMAT_R8G8B8A8_SINT; }
    if (!strcmp(valueString, "DXGI_FORMAT_R16G16_TYPELESS") || !strcmp(valueString, "R16G16_TYPELESS")) { return DXGI_FORMAT_R16G16_TYPELESS; }
    if (!strcmp(valueString, "DXGI_FORMAT_R16G16_FLOAT") || !strcmp(valueString, "R16G16_FLOAT")) { return DXGI_FORMAT_R16G16_FLOAT; }
    if (!strcmp(valueString, "DXGI_FORMAT_R16G16_UNORM") || !strcmp(valueString, "R16G16_UNORM")) { return DXGI_FORMAT_R16G16_UNORM; }
    if (!strcmp(valueString, "DXGI_FORMAT_R16G16_UINT") || !strcmp(valueString, "R16G16_UINT")) { return DXGI_FORMAT_R16G16_UINT; }
    if (!strcmp(valueString, "DXGI_FORMAT_R16G16_SNORM") || !strcmp(valueString, "R16G16_SNORM")) { return DXGI_FORMAT_R16G16_SNORM; }
    if (!strcmp(valueString, "DXGI_FORMAT_R16G16_SINT") || !strcmp(valueString, "R16G16_SINT")) { return DXGI_FORMAT_R16G16_SINT; }
    if (!strcmp(valueString, "DXGI_FORMAT_R32_TYPELESS") || !strcmp(valueString, "R32_TYPELESS")) { return DXGI_FORMAT_R32_TYPELESS; }
    if (!strcmp(valueString, "DXGI_FORMAT_D32_FLOAT") || !strcmp(valueString, "D32_FLOAT")) { return DXGI_FORMAT_D32_FLOAT; }
    if (!strcmp(valueString, "DXGI_FORMAT_R32_FLOAT") || !strcmp(valueString, "R32_FLOAT")) { return DXGI_FORMAT_R32_FLOAT; }
    if (!strcmp(valueString, "DXGI_FORMAT_R32_UINT") || !strcmp(valueString, "R32_UINT")) { return DXGI_FORMAT_R32_UINT; }
    if (!strcmp(valueString, "DXGI_FORMAT_R32_SINT") || !strcmp(valueString, "R32_SINT")) { return DXGI_FORMAT_R32_SINT; }
    if (!strcmp(valueString, "DXGI_FORMAT_R24G8_TYPELESS") || !strcmp(valueString, "R24G8_TYPELESS")) { return DXGI_FORMAT_R24G8_TYPELESS; }
    if (!strcmp(valueString, "DXGI_FORMAT_D24_UNORM_S8_UINT") || !strcmp(valueString, "D24_UNORM_S8_UINT")) { return DXGI_FORMAT_D24_UNORM_S8_UINT; }
    if (!strcmp(valueString, "DXGI_FORMAT_R24_UNORM_X8_TYPELESS") || !strcmp(valueString, "R24_UNORM_X8_TYPELESS")) { return DXGI_FORMAT_R24_UNORM_X8_TYPELESS; }
    if (!strcmp(valueString, "DXGI_FORMAT_X24_TYPELESS_G8_UINT") || !strcmp(valueString, "X24_TYPELESS_G8_UINT")) { return DXGI_FORMAT_X24_TYPELESS_G8_UINT; }
    if (!strcmp(valueString, "DXGI_FORMAT_R8G8_TYPELESS") || !strcmp(valueString, "R8G8_TYPELESS")) { return DXGI_FORMAT_R8G8_TYPELESS; }
    if (!strcmp(valueString, "DXGI_FORMAT_R8G8_UNORM") || !strcmp(valueString, "R8G8_UNORM")) { return DXGI_FORMAT_R8G8_UNORM; }
    if (!strcmp(valueString, "DXGI_FORMAT_R8G8_UINT") || !strcmp(valueString, "R8G8_UINT")) { return DXGI_FORMAT_R8G8_UINT; }
    if (!strcmp(valueString, "DXGI_FORMAT_R8G8_SNORM") || !strcmp(valueString, "R8G8_SNORM")) { return DXGI_FORMAT_R8G8_SNORM; }
    if (!strcmp(valueString, "DXGI_FORMAT_R8G8_SINT") || !strcmp(valueString, "R8G8_SINT")) { return DXGI_FORMAT_R8G8_SINT; }
    if (!strcmp(valueString, "DXGI_FORMAT_R16_TYPELESS") || !strcmp(valueString, "R16_TYPELESS")) { return DXGI_FORMAT_R16_TYPELESS; }
    if (!strcmp(valueString, "DXGI_FORMAT_R16_FLOAT") || !strcmp(valueString, "R16_FLOAT")) { return DXGI_FORMAT_R16_FLOAT; }
    if (!strcmp(valueString, "DXGI_FORMAT_D16_UNORM") || !strcmp(valueString, "D16_UNORM")) { return DXGI_FORMAT_D16_UNORM; }
    if (!strcmp(valueString, "DXGI_FORMAT_R16_UNORM") || !strcmp(valueString, "R16_UNORM")) { return DXGI_FORMAT_R16_UNORM; }
    if (!strcmp(valueString, "DXGI_FORMAT_R16_UINT") || !strcmp(valueString, "R16_UINT")) { return DXGI_FORMAT_R16_UINT; }
    if (!strcmp(valueString, "DXGI_FORMAT_R16_SNORM") || !strcmp(valueString, "R16_SNORM")) { return DXGI_FORMAT_R16_SNORM; }
    if (!strcmp(valueString, "DXGI_FORMAT_R16_SINT") || !strcmp(valueString, "R16_SINT")) { return DXGI_FORMAT_R16_SINT; }
    if (!strcmp(valueString, "DXGI_FORMAT_R8_TYPELESS") || !strcmp(valueString, "R8_TYPELESS")) { return DXGI_FORMAT_R8_TYPELESS; }
    if (!strcmp(valueString, "DXGI_FORMAT_R8_UNORM") || !strcmp(valueString, "R8_UNORM")) { return DXGI_FORMAT_R8_UNORM; }
    if (!strcmp(valueString, "DXGI_FORMAT_R8_UINT") || !strcmp(valueString, "R8_UINT")) { return DXGI_FORMAT_R8_UINT; }
    if (!strcmp(valueString, "DXGI_FORMAT_R8_SNORM") || !strcmp(valueString, "R8_SNORM")) { return DXGI_FORMAT_R8_SNORM; }
    if (!strcmp(valueString, "DXGI_FORMAT_R8_SINT") || !strcmp(valueString, "R8_SINT")) { return DXGI_FORMAT_R8_SINT; }
    if (!strcmp(valueString, "DXGI_FORMAT_A8_UNORM") || !strcmp(valueString, "A8_UNORM")) { return DXGI_FORMAT_A8_UNORM; }
    if (!strcmp(valueString, "DXGI_FORMAT_R1_UNORM") || !strcmp(valueString, "R1_UNORM")) { return DXGI_FORMAT_R1_UNORM; }
    if (!strcmp(valueString, "DXGI_FORMAT_R9G9B9E5_SHAREDEXP") || !strcmp(valueString, "R9G9B9E5_SHAREDEXP")) { return DXGI_FORMAT_R9G9B9E5_SHAREDEXP; }
    if (!strcmp(valueString, "DXGI_FORMAT_R8G8_B8G8_UNORM") || !strcmp(valueString, "R8G8_B8G8_UNORM")) { return DXGI_FORMAT_R8G8_B8G8_UNORM; }
    if (!strcmp(valueString, "DXGI_FORMAT_G8R8_G8B8_UNORM") || !strcmp(valueString, "G8R8_G8B8_UNORM")) { return DXGI_FORMAT_G8R8_G8B8_UNORM; }
    if (!strcmp(valueString, "DXGI_FORMAT_BC1_TYPELESS") || !strcmp(valueString, "BC1_TYPELESS")) { return DXGI_FORMAT_BC1_TYPELESS; }
    if (!strcmp(valueString, "DXGI_FORMAT_BC1_UNORM") || !strcmp(valueString, "BC1_UNORM")) { return DXGI_FORMAT_BC1_UNORM; }
    if (!strcmp(valueString, "DXGI_FORMAT_BC1_UNORM_SRGB") || !strcmp(valueString, "BC1_UNORM_SRGB")) { return DXGI_FORMAT_BC1_UNORM_SRGB; }
    if (!strcmp(valueString, "DXGI_FORMAT_BC2_TYPELESS") || !strcmp(valueString, "BC2_TYPELESS")) { return DXGI_FORMAT_BC2_TYPELESS; }
    if (!strcmp(valueString, "DXGI_FORMAT_BC2_UNORM") || !strcmp(valueString, "BC2_UNORM")) { return DXGI_FORMAT_BC2_UNORM; }
    if (!strcmp(valueString, "DXGI_FORMAT_BC2_UNORM_SRGB") || !strcmp(valueString, "BC2_UNORM_SRGB")) { return DXGI_FORMAT_BC2_UNORM_SRGB; }
    if (!strcmp(valueString, "DXGI_FORMAT_BC3_TYPELESS") || !strcmp(valueString, "BC3_TYPELESS")) { return DXGI_FORMAT_BC3_TYPELESS; }
    if (!strcmp(valueString, "DXGI_FORMAT_BC3_UNORM") || !strcmp(valueString, "BC3_UNORM")) { return DXGI_FORMAT_BC3_UNORM; }
    if (!strcmp(valueString, "DXGI_FORMAT_BC3_UNORM_SRGB") || !strcmp(valueString, "BC3_UNORM_SRGB")) { return DXGI_FORMAT_BC3_UNORM_SRGB; }
    if (!strcmp(valueString, "DXGI_FORMAT_BC4_TYPELESS") || !strcmp(valueString, "BC4_TYPELESS")) { return DXGI_FORMAT_BC4_TYPELESS; }
    if (!strcmp(valueString, "DXGI_FORMAT_BC4_UNORM") || !strcmp(valueString, "BC4_UNORM")) { return DXGI_FORMAT_BC4_UNORM; }
    if (!strcmp(valueString, "DXGI_FORMAT_BC4_SNORM") || !strcmp(valueString, "BC4_SNORM")) { return DXGI_FORMAT_BC4_SNORM; }
    if (!strcmp(valueString, "DXGI_FORMAT_BC5_TYPELESS") || !strcmp(valueString, "BC5_TYPELESS")) { return DXGI_FORMAT_BC5_TYPELESS; }
    if (!strcmp(valueString, "DXGI_FORMAT_BC5_UNORM") || !strcmp(valueString, "BC5_UNORM")) { return DXGI_FORMAT_BC5_UNORM; }
    if (!strcmp(valueString, "DXGI_FORMAT_BC5_SNORM") || !strcmp(valueString, "BC5_SNORM")) { return DXGI_FORMAT_BC5_SNORM; }
    if (!strcmp(valueString, "DXGI_FORMAT_B5G6R5_UNORM") || !strcmp(valueString, "B5G6R5_UNORM")) { return DXGI_FORMAT_B5G6R5_UNORM; }
    if (!strcmp(valueString, "DXGI_FORMAT_B5G5R5A1_UNORM") || !strcmp(valueString, "B5G5R5A1_UNORM")) { return DXGI_FORMAT_B5G5R5A1_UNORM; }
    if (!strcmp(valueString, "DXGI_FORMAT_B8G8R8A8_UNORM") || !strcmp(valueString, "B8G8R8A8_UNORM")) { return DXGI_FORMAT_B8G8R8A8_UNORM; }
    if (!strcmp(valueString, "DXGI_FORMAT_B8G8R8X8_UNORM") || !strcmp(valueString, "B8G8R8X8_UNORM")) { return DXGI_FORMAT_B8G8R8X8_UNORM; }
    if (!strcmp(valueString, "DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM") || !strcmp(valueString, "R10G10B10_XR_BIAS_A2_UNORM")) { return DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM; }
    if (!strcmp(valueString, "DXGI_FORMAT_B8G8R8A8_TYPELESS") || !strcmp(valueString, "B8G8R8A8_TYPELESS")) { return DXGI_FORMAT_B8G8R8A8_TYPELESS; }
    if (!strcmp(valueString, "DXGI_FORMAT_B8G8R8A8_UNORM_SRGB") || !strcmp(valueString, "B8G8R8A8_UNORM_SRGB")) { return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB; }
    if (!strcmp(valueString, "DXGI_FORMAT_B8G8R8X8_TYPELESS") || !strcmp(valueString, "B8G8R8X8_TYPELESS")) { return DXGI_FORMAT_B8G8R8X8_TYPELESS; }
    if (!strcmp(valueString, "DXGI_FORMAT_B8G8R8X8_UNORM_SRGB") || !strcmp(valueString, "B8G8R8X8_UNORM_SRGB")) { return DXGI_FORMAT_B8G8R8X8_UNORM_SRGB; }
    if (!strcmp(valueString, "DXGI_FORMAT_BC6H_TYPELESS") || !strcmp(valueString, "BC6H_TYPELESS")) { return DXGI_FORMAT_BC6H_TYPELESS; }
    if (!strcmp(valueString, "DXGI_FORMAT_BC6H_UF16") || !strcmp(valueString, "BC6H_UF16")) { return DXGI_FORMAT_BC6H_UF16; }
    if (!strcmp(valueString, "DXGI_FORMAT_BC6H_SF16") || !strcmp(valueString, "BC6H_SF16")) { return DXGI_FORMAT_BC6H_SF16; }
    if (!strcmp(valueString, "DXGI_FORMAT_BC7_TYPELESS") || !strcmp(valueString, "BC7_TYPELESS")) { return DXGI_FORMAT_BC7_TYPELESS; }
    if (!strcmp(valueString, "DXGI_FORMAT_BC7_UNORM") || !strcmp(valueString, "BC7_UNORM")) { return DXGI_FORMAT_BC7_UNORM; }
    if (!strcmp(valueString, "DXGI_FORMAT_BC7_UNORM_SRGB") || !strcmp(valueString, "BC7_UNORM_SRGB")) { return DXGI_FORMAT_BC7_UNORM_SRGB; }
    if (!strcmp(valueString, "DXGI_FORMAT_AYUV") || !strcmp(valueString, "AYUV")) { return DXGI_FORMAT_AYUV; }
    if (!strcmp(valueString, "DXGI_FORMAT_Y410") || !strcmp(valueString, "Y410")) { return DXGI_FORMAT_Y410; }
    if (!strcmp(valueString, "DXGI_FORMAT_Y416") || !strcmp(valueString, "Y416")) { return DXGI_FORMAT_Y416; }
    if (!strcmp(valueString, "DXGI_FORMAT_NV12") || !strcmp(valueString, "NV12")) { return DXGI_FORMAT_NV12; }
    if (!strcmp(valueString, "DXGI_FORMAT_P010") || !strcmp(valueString, "P010")) { return DXGI_FORMAT_P010; }
    if (!strcmp(valueString, "DXGI_FORMAT_P016") || !strcmp(valueString, "P016")) { return DXGI_FORMAT_P016; }
    if (!strcmp(valueString, "DXGI_FORMAT_420_OPAQUE") || !strcmp(valueString, "420_OPAQUE")) { return DXGI_FORMAT_420_OPAQUE; }
    if (!strcmp(valueString, "DXGI_FORMAT_YUY2") || !strcmp(valueString, "YUY2")) { return DXGI_FORMAT_YUY2; }
    if (!strcmp(valueString, "DXGI_FORMAT_Y210") || !strcmp(valueString, "Y210")) { return DXGI_FORMAT_Y210; }
    if (!strcmp(valueString, "DXGI_FORMAT_Y216") || !strcmp(valueString, "Y216")) { return DXGI_FORMAT_Y216; }
    if (!strcmp(valueString, "DXGI_FORMAT_NV11") || !strcmp(valueString, "NV11")) { return DXGI_FORMAT_NV11; }
    if (!strcmp(valueString, "DXGI_FORMAT_AI44") || !strcmp(valueString, "AI44")) { return DXGI_FORMAT_AI44; }
    if (!strcmp(valueString, "DXGI_FORMAT_IA44") || !strcmp(valueString, "IA44")) { return DXGI_FORMAT_IA44; }
    if (!strcmp(valueString, "DXGI_FORMAT_P8") || !strcmp(valueString, "P8")) { return DXGI_FORMAT_P8; }
    if (!strcmp(valueString, "DXGI_FORMAT_A8P8") || !strcmp(valueString, "A8P8")) { return DXGI_FORMAT_A8P8; }
    if (!strcmp(valueString, "DXGI_FORMAT_B4G4R4A4_UNORM") || !strcmp(valueString, "B4G4R4A4_UNORM")) { return DXGI_FORMAT_B4G4R4A4_UNORM; }

    throw std::invalid_argument("Unrecognized DXGI format");
}

template <typename TReturn>
TReturn ParseFieldHelper(
    const rapidjson::Value& object, 
    std::string_view fieldName, 
    bool required,
    TReturn defaultValue,
    std::function<TReturn(const rapidjson::Value&)> func)
{
    auto fieldIterator = object.FindMember(fieldName.data());
    if (fieldIterator == object.MemberEnd())
    {
        if (required)
        { 
            throw std::invalid_argument(fmt::format("Field '{}' is required.", fieldName)); 
        }
        return defaultValue;
    }

    try
    {
        return func(fieldIterator->value);
    }
    catch (const std::exception& e)
    {
        throw std::invalid_argument(fmt::format("Error parsing field '{}': {}", fieldName, e.what()));
    }
}

template <typename T>
T ParseFloatingPointNumber(const rapidjson::Value& value)
{
    static_assert(std::is_floating_point_v<T> || std::is_same_v<T, half_float::half>);
    if (value.IsFloat() || value.IsDouble() || value.IsLosslessDouble())
    {
        return static_cast<T>(value.GetDouble());
    }
    else if (value.IsString())
    {
        auto strValue = value.GetString();
        if (!_stricmp(strValue, "inf")) { return std::numeric_limits<T>::infinity(); }
        if (!_stricmp(strValue, "-inf")) { return -std::numeric_limits<T>::infinity(); }
        if (!_stricmp(strValue, "nan")) { return std::numeric_limits<T>::quiet_NaN(); }
        throw std::invalid_argument("Expected 'NaN', 'Inf', or '-Inf'.");
    }
    else
    {
        throw std::invalid_argument("Expected a number or 'NaN', 'Inf', or '-Inf'.");
    }
}

template <typename T> 
gsl::span<T> ParseArray(
    const rapidjson::Value& value, 
    BucketAllocator& allocator, 
    std::function<T(const rapidjson::Value&)> elementParser)
{
    if (value.GetType() != rapidjson::Type::kArrayType)
    {
        throw std::invalid_argument("Expected an array.");
    }

    auto valueArray = value.GetArray();
    auto outputElements = allocator.Allocate<T>(valueArray.Size());
    for (uint32_t i = 0; i < valueArray.Size(); i++)
    {
        outputElements[i] = elementParser(valueArray[i]);
    }

    return gsl::make_span(outputElements, valueArray.Size());
}

template <typename T> 
std::vector<T> ParseArrayAsVector(
    const rapidjson::Value& value, 
    std::function<T(const rapidjson::Value&)> elementParser)
{
    if (value.GetType() != rapidjson::Type::kArrayType)
    {
        throw std::invalid_argument("Expected an array.");
    }

    auto valueArray = value.GetArray();
    std::vector<T> outputElements(valueArray.Size());
    for (uint32_t i = 0; i < valueArray.Size(); i++)
    {
        outputElements[i] = elementParser(valueArray[i]);
    }

    return outputElements;
}

template <typename T>
std::vector<std::byte> ParseArrayAsBytes(
    const rapidjson::Value& value,
    std::function<T(const rapidjson::Value&)> elementParser)
{
    if (value.GetType() != rapidjson::Type::kArrayType)
    {
        throw std::invalid_argument("Expected an array.");
    }

    std::vector<std::byte> output;
    for (auto& element : value.GetArray())
    {
        T elementValue = elementParser(element);
        for (auto byte : gsl::as_bytes(gsl::make_span<T>(&elementValue, 1)))
        {
            output.push_back(byte);
        }
    }

    return output;
}

// Helper for parsing flags from a JSON string or array. Flags are enums that may be bitwise-OR'd together
// to create a mask. This helper takes a function that parses a single flag value from a JSON string value.
// For example, if the flag enums are defined as "enum FOO {A=0x1,B=0x2,C=x4}" then the parser should be
// able to convert "A" to A, "B" to B, and "C" to C.
template <typename T>
T ParseFlags(const rapidjson::Value& value, std::function<T(const rapidjson::Value& value)> flagParser)
{
    if (value.GetType() == rapidjson::Type::kStringType)
    {
        return flagParser(value);
    }
    else if (value.GetType() == rapidjson::Type::kArrayType)
    {
        T flags = {};
        for (auto& elementValue : value.GetArray())
        {
            flags |= flagParser(elementValue);
        }
        return flags;
    }

    throw std::invalid_argument("Expected a string or an array of strings.");
}

template <typename T>
T* AsPointer(gsl::span<T> s) 
{ 
    return s.empty() ? nullptr : s.data();
}

namespace JsonParsers
{
// ----------------------------------------------------------------------------
// STRING
// ----------------------------------------------------------------------------

std::string ParseString(const rapidjson::Value& value)
{
    if (!value.IsString())
    {
        throw std::invalid_argument("Expected a string.");
    }
    return value.GetString();
}

std::string ParseStringField(const rapidjson::Value& object, std::string_view fieldName, bool required = true, std::string defaultValue = {})
{
    return ParseFieldHelper<std::string>(object, fieldName, required, defaultValue, [](auto& value){ 
        return ParseString(value); 
    });
}

// ----------------------------------------------------------------------------
// BOOL
// ----------------------------------------------------------------------------

bool ParseBool(const rapidjson::Value& value)
{
    if (!value.IsBool())
    {
        throw std::invalid_argument("Expected a bool.");
    }
    return value.GetBool();
}

bool ParseBoolField(const rapidjson::Value& object, std::string_view fieldName, bool required, bool defaultValue)
{
    return ParseFieldHelper<bool>(object, fieldName, required, defaultValue, [](auto& value){ 
        return ParseBool(value); 
    });
}

// ----------------------------------------------------------------------------
// FLOAT16
// ----------------------------------------------------------------------------

half_float::half ParseFloat16(const rapidjson::Value& value)
{
    auto parsedValue = ParseFloatingPointNumber<float>(value);
    return half_float::half(parsedValue);
}

half_float::half ParseFloat16Field(const rapidjson::Value& object, std::string_view fieldName, bool required, half_float::half defaultValue)
{
    return ParseFieldHelper<half_float::half>(object, fieldName, required, defaultValue, [](auto& value){ 
        return ParseFloat16(value); 
    });
}

gsl::span<half_float::half> ParseFloat16Array(const rapidjson::Value& value, BucketAllocator& allocator)
{
    return ParseArray<half_float::half>(value, allocator, ParseFloat16);
}

gsl::span<half_float::half> ParseFloat16ArrayField(const rapidjson::Value& object, std::string_view fieldName, BucketAllocator& allocator, bool required, gsl::span<half_float::half> defaultValue)
{
    return ParseFieldHelper<gsl::span<half_float::half>>(object, fieldName, required, defaultValue, [&allocator](auto& value){ 
        return ParseFloat16Array(value, allocator); 
    });
}

// ----------------------------------------------------------------------------
// FLOAT32
// ----------------------------------------------------------------------------

float ParseFloat32(const rapidjson::Value& value)
{
    return ParseFloatingPointNumber<float>(value);
}

float ParseFloat32Field(const rapidjson::Value& object, std::string_view fieldName, bool required, float defaultValue)
{
    return ParseFieldHelper<float>(object, fieldName, required, defaultValue, [](auto& value){ 
        return ParseFloat32(value); 
    });
}

gsl::span<float> ParseFloat32Array(const rapidjson::Value& value, BucketAllocator& allocator)
{
    return ParseArray<float>(value, allocator, ParseFloat32);
}

gsl::span<float> ParseFloat32ArrayField(const rapidjson::Value& object, std::string_view fieldName, BucketAllocator& allocator, bool required, gsl::span<float> defaultValue)
{
    return ParseFieldHelper<gsl::span<float>>(object, fieldName, required, defaultValue, [&allocator](auto& value){ 
        return ParseFloat32Array(value, allocator); 
    });
}

// ----------------------------------------------------------------------------
// FLOAT64
// ----------------------------------------------------------------------------

double ParseFloat64(const rapidjson::Value& value)
{
    return ParseFloatingPointNumber<double>(value);
}

double ParseFloat64Field(const rapidjson::Value& object, std::string_view fieldName, bool required, double defaultValue)
{
    return ParseFieldHelper<double>(object, fieldName, required, defaultValue, [](auto& value){ 
        return ParseFloat64(value); 
    });
}

// ----------------------------------------------------------------------------
// INT8
// ----------------------------------------------------------------------------

int8_t ParseInt8(const rapidjson::Value& value)
{
    if (!value.IsInt64())
    {
        throw std::invalid_argument("Expected a signed integer.");
    }
    return gsl::narrow<int8_t>(value.GetInt64());
}

int8_t ParseInt8Field(const rapidjson::Value& object, std::string_view fieldName, bool required, int8_t defaultValue)
{
    return ParseFieldHelper<int8_t>(object, fieldName, required, defaultValue, [](auto& value){ 
        return ParseInt8(value); 
    });
}

// ----------------------------------------------------------------------------
// INT16
// ----------------------------------------------------------------------------

int16_t ParseInt16(const rapidjson::Value& value)
{
    if (!value.IsInt64())
    {
        throw std::invalid_argument("Expected a signed integer.");
    }
    return gsl::narrow<int16_t>(value.GetInt64());
}

int16_t ParseInt16Field(const rapidjson::Value& object, std::string_view fieldName, bool required, int16_t defaultValue)
{
    return ParseFieldHelper<int16_t>(object, fieldName, required, defaultValue, [](auto& value){ 
        return ParseInt16(value); 
    });
}

// ----------------------------------------------------------------------------
// INT32
// ----------------------------------------------------------------------------

int32_t ParseInt32(const rapidjson::Value& value)
{
    if (!value.IsInt64())
    {
        throw std::invalid_argument("Expected a signed integer.");
    }
    return gsl::narrow<int32_t>(value.GetInt64());
}

int32_t ParseInt32Field(const rapidjson::Value& object, std::string_view fieldName, bool required, int32_t defaultValue)
{
    return ParseFieldHelper<int32_t>(object, fieldName, required, defaultValue, [](auto& value){ 
        return ParseInt32(value); 
    });
}

gsl::span<int32_t> ParseInt32Array(const rapidjson::Value& value, BucketAllocator& allocator)
{
    return ParseArray<int32_t>(value, allocator, ParseInt32);
}

gsl::span<int32_t> ParseInt32ArrayField(const rapidjson::Value& object, std::string_view fieldName, BucketAllocator& allocator, bool required, gsl::span<int32_t> defaultValue)
{
    return ParseFieldHelper<gsl::span<int32_t>>(object, fieldName, required, defaultValue, [&allocator](auto& value){ 
        return ParseInt32Array(value, allocator); 
    });
}

// ----------------------------------------------------------------------------
// INT64
// ----------------------------------------------------------------------------

int64_t ParseInt64(const rapidjson::Value& value)
{
    if (!value.IsInt64())
    {
        throw std::invalid_argument("Expected a signed integer.");
    }
    return value.GetInt64();
}

int64_t ParseInt64Field(const rapidjson::Value& object, std::string_view fieldName, bool required, int64_t defaultValue)
{
    return ParseFieldHelper<int64_t>(object, fieldName, required, defaultValue, [](auto& value){ 
        return ParseInt64(value); 
    });
}

std::vector<int64_t> ParseInt64ArrayAsVector(const rapidjson::Value& object)
{
    return ParseArrayAsVector<int64_t>(object, ParseInt64);
}

std::vector<int64_t> ParseInt64ArrayAsVectorField(const rapidjson::Value& object, std::string_view fieldName, bool required, std::vector<int64_t> defaultValue)
{
    return ParseFieldHelper<std::vector<int64_t>>(object, fieldName, required, defaultValue, [](auto& value){ 
        return ParseInt64ArrayAsVector(value); 
    });
}

// ----------------------------------------------------------------------------
// UINT8
// ----------------------------------------------------------------------------

uint8_t ParseUInt8(const rapidjson::Value& value)
{
    if (!value.IsUint64())
    {
        throw std::invalid_argument("Expected an unsigned integer.");
    }
    return gsl::narrow<uint8_t>(value.GetUint64());
}

uint8_t ParseUInt8Field(const rapidjson::Value& object, std::string_view fieldName, bool required, uint8_t defaultValue)
{
    return ParseFieldHelper<uint8_t>(object, fieldName, required, defaultValue, [](auto& value){ 
        return ParseUInt8(value); 
    });
}

// ----------------------------------------------------------------------------
// UINT16
// ----------------------------------------------------------------------------

uint16_t ParseUInt16(const rapidjson::Value& value)
{
    if (!value.IsUint64())
    {
        throw std::invalid_argument("Expected an unsigned integer.");
    }
    return gsl::narrow<uint16_t>(value.GetUint64());
}

uint16_t ParseUInt16Field(const rapidjson::Value& object, std::string_view fieldName, bool required, uint16_t defaultValue)
{
    return ParseFieldHelper<uint16_t>(object, fieldName, required, defaultValue, [](auto& value){ 
        return ParseUInt16(value); 
    });
}

// ----------------------------------------------------------------------------
// UINT32
// ----------------------------------------------------------------------------

uint32_t ParseUInt32(const rapidjson::Value& value)
{
    if (!value.IsUint64())
    {
        throw std::invalid_argument("Expected an unsigned integer.");
    }
    return gsl::narrow<uint32_t>(value.GetUint64());
}

uint32_t ParseUInt32Field(const rapidjson::Value& object, std::string_view fieldName, bool required, uint32_t defaultValue)
{
    return ParseFieldHelper<uint32_t>(object, fieldName, required, defaultValue, [](auto& value){ 
        return ParseUInt32(value); 
    });
}

gsl::span<uint32_t> ParseUInt32Array(const rapidjson::Value& value, BucketAllocator& allocator)
{
    return ParseArray<uint32_t>(value, allocator, ParseUInt32);
}

gsl::span<uint32_t> ParseUInt32ArrayField(const rapidjson::Value& object, std::string_view fieldName, BucketAllocator& allocator, bool required, gsl::span<uint32_t> defaultValue)
{
    return ParseFieldHelper<gsl::span<uint32_t>>(object, fieldName, required, defaultValue, [&allocator](auto& value){ 
        return ParseUInt32Array(value, allocator); 
    });
}

std::vector<uint32_t> ParseUInt32ArrayAsVector(const rapidjson::Value& object)
{
    return ParseArrayAsVector<uint32_t>(object, ParseUInt32);
}

std::vector<uint32_t> ParseUInt32ArrayAsVectorField(const rapidjson::Value& object, std::string_view fieldName, bool required, std::vector<uint32_t> defaultValue)
{
    return ParseFieldHelper<std::vector<uint32_t>>(object, fieldName, required, defaultValue, [](auto& value){ 
        return ParseUInt32ArrayAsVector(value); 
    });
}

// ----------------------------------------------------------------------------
// UINT64
// ----------------------------------------------------------------------------

uint64_t ParseUInt64(const rapidjson::Value& value)
{
    if (!value.IsUint64())
    {
        throw std::invalid_argument("Expected an unsigned integer.");
    }
    return value.GetUint64();
}

uint64_t ParseUInt64Field(const rapidjson::Value& object, std::string_view fieldName, bool required, uint64_t defaultValue)
{
    return ParseFieldHelper<uint64_t>(object, fieldName, required, defaultValue, [](auto& value){ 
        return ParseUInt64(value); 
    });
}

// ----------------------------------------------------------------------------
// Mixed Primitives
// ----------------------------------------------------------------------------

template <typename T>
void PushBytes(const T& value, std::vector<std::byte>& outputBuffer)
{
    for (auto& byte : gsl::as_bytes(gsl::make_span<const T>(&value, 1)))
    {
        outputBuffer.push_back(byte);
    }
}

std::vector<std::byte> ParseMixedPrimitiveArray(const rapidjson::Value& object)
{
    if (object.GetType() != rapidjson::Type::kArrayType)
    {
        throw std::invalid_argument("Expected an array.");
    }

    std::vector<std::byte> data;
    for (auto& element : object.GetArray())
    {
        if (element.GetType() != rapidjson::Type::kObjectType)
        {
            throw std::invalid_argument("Expected an object.");
        }

        auto elementType = ParseDmlTensorDataTypeField(element, "type");
        switch (elementType)
        {
        case DML_TENSOR_DATA_TYPE_FLOAT32: PushBytes(ParseFloat32Field(element, "value"), data); break;
        case DML_TENSOR_DATA_TYPE_FLOAT64: PushBytes(ParseFloat64Field(element, "value"), data); break;
        case DML_TENSOR_DATA_TYPE_UINT8: PushBytes(ParseUInt8Field(element, "value"), data); break;
        case DML_TENSOR_DATA_TYPE_UINT16: PushBytes(ParseUInt16Field(element, "value"), data); break;
        case DML_TENSOR_DATA_TYPE_UINT32: PushBytes(ParseUInt32Field(element, "value"), data); break;
        case DML_TENSOR_DATA_TYPE_UINT64: PushBytes(ParseUInt64Field(element, "value"), data); break;
        case DML_TENSOR_DATA_TYPE_INT8: PushBytes(ParseInt8Field(element, "value"), data); break;
        case DML_TENSOR_DATA_TYPE_INT16: PushBytes(ParseInt16Field(element, "value"), data); break;
        case DML_TENSOR_DATA_TYPE_INT32: PushBytes(ParseInt32Field(element, "value"), data); break;
        case DML_TENSOR_DATA_TYPE_INT64: PushBytes(ParseInt64Field(element, "value"), data); break;
        default: throw std::invalid_argument("Data type not supported.");
        }
    }

    return data;
}

// ----------------------------------------------------------------------------
// DML_SIZE_2D
// ----------------------------------------------------------------------------
static void ParseDmlSize2d(const rapidjson::Value& value, DML_SIZE_2D& returnValue)
{
    if (!value.IsObject())
    {
        throw std::invalid_argument("Expected a non-null JSON object.");
    }
    returnValue.Width = ParseUInt32Field(value, "Width");
    returnValue.Height = ParseUInt32Field(value, "Height");
}

DML_SIZE_2D ParseDmlSize2d(const rapidjson::Value& value)
{
    DML_SIZE_2D returnValue = {};
    ParseDmlSize2d(value, returnValue);
    return returnValue;
}

DML_SIZE_2D* ParseDmlSize2d(const rapidjson::Value& value, BucketAllocator& allocator)
{
    auto returnValue = allocator.Allocate<DML_SIZE_2D>();
    ParseDmlSize2d(value, *returnValue);
    return returnValue;
}

DML_SIZE_2D* ParseDmlSize2dField(const rapidjson::Value& object, std::string_view fieldName, BucketAllocator& allocator, bool required, DML_SIZE_2D* defaultValue)
{
    return ParseFieldHelper<DML_SIZE_2D*>(object, fieldName, required, defaultValue, [&allocator](auto& value){ 
        return ParseDmlSize2d(value, allocator); 
    });
}

// ----------------------------------------------------------------------------
// DML_SCALAR_UNION
// ----------------------------------------------------------------------------
static void ParseDmlScalarUnion(const rapidjson::Value& value, DML_TENSOR_DATA_TYPE dataType, DML_SCALAR_UNION& returnValue)
{
    if (value.IsObject())
    {
        switch (dataType)
        {
        case DML_TENSOR_DATA_TYPE_FLOAT16: returnValue.UInt16 = ParseUInt16Field(value, "UInt16"); break;
        case DML_TENSOR_DATA_TYPE_FLOAT32: returnValue.Float32 = ParseFloat32Field(value, "Float32"); break;
        case DML_TENSOR_DATA_TYPE_FLOAT64: returnValue.Float64 = ParseFloat64Field(value, "Float64"); break;
        case DML_TENSOR_DATA_TYPE_UINT8: returnValue.UInt8 = ParseUInt8Field(value, "UInt8"); break;
        case DML_TENSOR_DATA_TYPE_UINT16: returnValue.UInt16 = ParseUInt16Field(value, "UInt16"); break;
        case DML_TENSOR_DATA_TYPE_UINT32: returnValue.UInt32 = ParseUInt32Field(value, "UInt32"); break;
        case DML_TENSOR_DATA_TYPE_UINT64: returnValue.UInt64 = ParseUInt64Field(value, "UInt64"); break;
        case DML_TENSOR_DATA_TYPE_INT8: returnValue.Int8 = ParseInt8Field(value, "Int8"); break;
        case DML_TENSOR_DATA_TYPE_INT16: returnValue.Int16 = ParseInt16Field(value, "Int16"); break;
        case DML_TENSOR_DATA_TYPE_INT32: returnValue.Int32 = ParseInt32Field(value, "Int32"); break;
        case DML_TENSOR_DATA_TYPE_INT64: returnValue.Int64 = ParseInt64Field(value, "Int64"); break;
        default: throw std::invalid_argument("Data type not supported for DML_SCALAR_UNION.");
        }
    }
    else if (value.IsNumber())
    {
        switch (dataType)
        {
        case DML_TENSOR_DATA_TYPE_FLOAT16:
        {
            auto halfValue = ParseFloat16(value);
            returnValue.UInt16 = *reinterpret_cast<const uint16_t*>(&halfValue);
            break;
        }
        case DML_TENSOR_DATA_TYPE_FLOAT32: returnValue.Float32 = ParseFloat32(value); break;
        case DML_TENSOR_DATA_TYPE_FLOAT64: returnValue.Float64 = ParseFloat64(value); break;
        case DML_TENSOR_DATA_TYPE_UINT8: returnValue.UInt8 = ParseUInt8(value); break;
        case DML_TENSOR_DATA_TYPE_UINT16: returnValue.UInt16 = ParseUInt16(value); break;
        case DML_TENSOR_DATA_TYPE_UINT32: returnValue.UInt32 = ParseUInt32(value); break;
        case DML_TENSOR_DATA_TYPE_UINT64: returnValue.UInt64 = ParseUInt64(value); break;
        case DML_TENSOR_DATA_TYPE_INT8: returnValue.Int8 = ParseInt8(value); break;
        case DML_TENSOR_DATA_TYPE_INT16: returnValue.Int16 = ParseInt16(value); break;
        case DML_TENSOR_DATA_TYPE_INT32: returnValue.Int32 = ParseInt32(value); break;
        case DML_TENSOR_DATA_TYPE_INT64: returnValue.Int64 = ParseInt64(value); break;
        default: throw std::invalid_argument("Data type not supported for DML_SCALAR_UNION.");
        }
    }
    else
    {
        throw std::invalid_argument("Expected a non-null JSON object or number.");
    }
}

DML_SCALAR_UNION ParseDmlScalarUnion(const rapidjson::Value& value, DML_TENSOR_DATA_TYPE dataType)
{
    DML_SCALAR_UNION returnValue{};
    ParseDmlScalarUnion(value, dataType, returnValue);
    return returnValue;
}

DML_SCALAR_UNION* ParseDmlScalarUnion(const rapidjson::Value& value, DML_TENSOR_DATA_TYPE dataType, BucketAllocator& allocator)
{
    auto returnValue = allocator.Allocate<DML_SCALAR_UNION>();
    ParseDmlScalarUnion(value, dataType, *returnValue);
    return returnValue;
}

DML_SCALAR_UNION* ParseDmlScalarUnionField(
    const rapidjson::Value& object, 
    std::string_view scalarUnionFieldName, 
    std::string_view dataTypeFieldName,
    BucketAllocator& allocator, 
    bool required,
    DML_SCALAR_UNION* defaultValue)
{
    auto dataType = ParseDmlTensorDataTypeField(object, dataTypeFieldName, required);
    return ParseFieldHelper<DML_SCALAR_UNION*>(object, scalarUnionFieldName, required, defaultValue, [=, &allocator](auto& value){ 
        return ParseDmlScalarUnion(value, dataType, allocator); 
    });
}

// ----------------------------------------------------------------------------
// DML_SCALE_BIAS
// ----------------------------------------------------------------------------
static void ParseDmlScaleBias(const rapidjson::Value& value, DML_SCALE_BIAS& returnValue)
{
    if (!value.IsObject())
    {
        throw std::invalid_argument("Expected a non-null JSON object.");
    }
    returnValue.Scale = ParseFloat32Field(value, "Scale");
    returnValue.Bias = ParseFloat32Field(value, "Bias");
}

DML_SCALE_BIAS ParseDmlScaleBias(const rapidjson::Value& value)
{
    DML_SCALE_BIAS returnValue{};
    ParseDmlScaleBias(value, returnValue);
    return returnValue;
}

DML_SCALE_BIAS* ParseDmlScaleBias(const rapidjson::Value& value, BucketAllocator& allocator)
{
    auto returnValue = allocator.Allocate<DML_SCALE_BIAS>();
    ParseDmlScaleBias(value, *returnValue);
    return returnValue;
}

DML_SCALE_BIAS* ParseDmlScaleBiasField(const rapidjson::Value& object, std::string_view fieldName, BucketAllocator& allocator, bool required, DML_SCALE_BIAS* defaultValue)
{
    return ParseFieldHelper<DML_SCALE_BIAS*>(object, fieldName, required, defaultValue, [&allocator](auto& value){ 
        return ParseDmlScaleBias(value, allocator); 
    });
}

// ----------------------------------------------------------------------------
// DML_BUFFER_TENSOR_DESC
// ----------------------------------------------------------------------------

DML_BUFFER_TENSOR_DESC* ParseDmlBufferTensorDesc(const rapidjson::Value& value, BucketAllocator& allocator)
{
    if (!value.IsObject())
    {
        throw std::invalid_argument("Expected a non-null JSON object.");
    }

    auto sizes = ParseUInt32ArrayField(value, "Sizes", allocator);
    auto strides = ParseUInt32ArrayField(value, "Strides", allocator, false);

    auto desc = allocator.Allocate<DML_BUFFER_TENSOR_DESC>();
    desc->DimensionCount = ParseUInt32Field(value, "DimensionCount", false, static_cast<uint32_t>(sizes.size()));
    desc->DataType = ParseDmlTensorDataTypeField(value, "DataType");
    desc->Flags = ParseDmlTensorFlagsField(value, "Flags", false, DML_TENSOR_FLAG_NONE);
    desc->Sizes = sizes.data();
    desc->Strides = strides.empty() ? nullptr : strides.data();
    desc->TotalTensorSizeInBytes = ParseUInt64Field(value, "TotalTensorSizeInBytes", false, 0);
    if (!desc->TotalTensorSizeInBytes)
    {
        desc->TotalTensorSizeInBytes = DMLCalcBufferTensorSize(
            desc->DataType,
            desc->DimensionCount,
            desc->Sizes,
            desc->Strides);
    }
    desc->GuaranteedBaseOffsetAlignment = ParseUInt32Field(value, "GuaranteedBaseOffsetAlignment", false, 0);

    return desc;
}

DML_BUFFER_TENSOR_DESC* ParseDmlBufferTensorDescField(const rapidjson::Value& object, std::string_view fieldName, BucketAllocator& allocator, bool required, DML_BUFFER_TENSOR_DESC* defaultValue)
{
    return ParseFieldHelper<DML_BUFFER_TENSOR_DESC*>(object, fieldName, required, defaultValue, [&allocator](auto& value){ 
        return ParseDmlBufferTensorDesc(value, allocator); 
    });
}

// ----------------------------------------------------------------------------
// DML_TENSOR_DESC
// ----------------------------------------------------------------------------

DML_TENSOR_DESC* ParseDmlTensorDesc(const rapidjson::Value& value, BucketAllocator& allocator)
{
    if (!value.IsObject())
    {
        throw std::invalid_argument("Expected a non-null JSON object.");
    }

    auto desc = allocator.Allocate<DML_TENSOR_DESC>();
    desc->Type = ParseDmlTensorTypeField(value, "Type", false, DML_TENSOR_TYPE_BUFFER);
    if (value.HasMember("Desc"))
    {
        desc->Desc = ParseDmlBufferTensorDesc(value["Desc"], allocator);
    }
    else
    {
        desc->Desc = ParseDmlBufferTensorDesc(value, allocator);
    }
    return desc;
}

DML_TENSOR_DESC* ParseDmlTensorDescField(const rapidjson::Value& object, std::string_view fieldName, BucketAllocator& allocator, bool required, DML_TENSOR_DESC* defaultValue)
{
    return ParseFieldHelper<DML_TENSOR_DESC*>(object, fieldName, required, defaultValue, [&allocator](auto& value){ 
        return ParseDmlTensorDesc(value, allocator); 
    });
}

gsl::span<DML_TENSOR_DESC> ParseDmlTensorDescArray(const rapidjson::Value& value, BucketAllocator& allocator)
{
    if (value.GetType() != rapidjson::Type::kArrayType)
    {
        throw std::invalid_argument("Expected an array.");
    }

    auto valueArray = value.GetArray();
    auto outputElements = allocator.Allocate<DML_TENSOR_DESC>(valueArray.Size());
    for (uint32_t i = 0; i < valueArray.Size(); i++)
    {
        outputElements[i] = *ParseDmlTensorDesc(valueArray[i], allocator);
    }

    return gsl::make_span(outputElements, valueArray.Size());
}

gsl::span<DML_TENSOR_DESC> ParseDmlTensorDescArrayField(const rapidjson::Value& object, std::string_view fieldName, BucketAllocator& allocator, bool required, gsl::span<DML_TENSOR_DESC> defaultValue)
{
    return ParseFieldHelper<gsl::span<DML_TENSOR_DESC>>(object, fieldName, required, defaultValue, [&allocator](auto& value){ 
        return ParseDmlTensorDescArray(value, allocator); 
    });
}

// ----------------------------------------------------------------------------
// OTHER
// ----------------------------------------------------------------------------

uint64_t GetTensorSize(const DML_TENSOR_DESC& desc)
{
    if (desc.Type == DML_TENSOR_TYPE_BUFFER)
    {
        return static_cast<const DML_BUFFER_TENSOR_DESC*>(desc.Desc)->TotalTensorSizeInBytes;
    }
    throw std::invalid_argument("Cannot determine size of invalid tensor desc.");
}

#include "JsonParsersGenerated.cpp"

std::vector<std::byte> GenerateInitialValuesFromList(DML_TENSOR_DATA_TYPE dataType, const rapidjson::Value& object)
{
    switch (dataType)
    {
    case DML_TENSOR_DATA_TYPE_FLOAT16: return ParseArrayAsBytes<half_float::half>(object, ParseFloat16);
    case DML_TENSOR_DATA_TYPE_FLOAT32: return ParseArrayAsBytes<float>(object, ParseFloat32);
    case DML_TENSOR_DATA_TYPE_FLOAT64: return ParseArrayAsBytes<double>(object, ParseFloat64);
    case DML_TENSOR_DATA_TYPE_UINT8: return ParseArrayAsBytes<uint8_t>(object, ParseUInt8);
    case DML_TENSOR_DATA_TYPE_UINT16: return ParseArrayAsBytes<uint16_t>(object, ParseUInt16);
    case DML_TENSOR_DATA_TYPE_UINT32: return ParseArrayAsBytes<uint32_t>(object, ParseUInt32);
    case DML_TENSOR_DATA_TYPE_UINT64: return ParseArrayAsBytes<uint64_t>(object, ParseUInt64);
    case DML_TENSOR_DATA_TYPE_INT8: return ParseArrayAsBytes<int8_t>(object, ParseInt8);
    case DML_TENSOR_DATA_TYPE_INT16: return ParseArrayAsBytes<int16_t>(object, ParseInt16);
    case DML_TENSOR_DATA_TYPE_INT32: return ParseArrayAsBytes<int32_t>(object, ParseInt32);
    case DML_TENSOR_DATA_TYPE_INT64: return ParseArrayAsBytes<int64_t>(object, ParseInt64);
    default: throw std::invalid_argument(fmt::format("Invalid tensor data type."));
    }
}

std::vector<std::byte> GenerateInitialValuesFromConstant(DML_TENSOR_DATA_TYPE dataType, const rapidjson::Value& object)
{
    auto valueCount = ParseUInt32Field(object, "valueCount");

    auto AsBytes = [=](auto value)->std::vector<std::byte>
    {
        std::vector<std::byte> valueBytes;
        for (auto& byte : gsl::as_bytes(gsl::make_span(&value, 1)))
        {
            valueBytes.push_back(byte);
        }

        std::vector<std::byte> allBytes(valueBytes.size() * valueCount);
        for (size_t i = 0; i < valueCount; i++)
        {
            std::copy(valueBytes.begin(), valueBytes.end(), allBytes.begin() + i * valueBytes.size());
        }
        return allBytes;
    };

    switch (dataType)
    {
    case DML_TENSOR_DATA_TYPE_FLOAT16: return AsBytes(ParseFloat16Field(object, "value"));
    case DML_TENSOR_DATA_TYPE_FLOAT32: return AsBytes(ParseFloat32Field(object, "value"));
    case DML_TENSOR_DATA_TYPE_FLOAT64: return AsBytes(ParseFloat64Field(object, "value"));
    case DML_TENSOR_DATA_TYPE_UINT8: return AsBytes(ParseUInt8Field(object, "value"));
    case DML_TENSOR_DATA_TYPE_UINT16: return AsBytes(ParseUInt16Field(object, "value"));
    case DML_TENSOR_DATA_TYPE_UINT32: return AsBytes(ParseUInt32Field(object, "value"));
    case DML_TENSOR_DATA_TYPE_UINT64: return AsBytes(ParseUInt64Field(object, "value"));
    case DML_TENSOR_DATA_TYPE_INT8: return AsBytes(ParseInt8Field(object, "value"));
    case DML_TENSOR_DATA_TYPE_INT16: return AsBytes(ParseInt16Field(object, "value"));
    case DML_TENSOR_DATA_TYPE_INT32: return AsBytes(ParseInt32Field(object, "value"));
    case DML_TENSOR_DATA_TYPE_INT64: return AsBytes(ParseInt64Field(object, "value"));
    default: throw std::invalid_argument(fmt::format("Invalid tensor data type."));
    }
}

std::vector<std::byte> GenerateInitialValuesFromSequence(DML_TENSOR_DATA_TYPE dataType, const rapidjson::Value& object)
{
    auto valueCount = ParseUInt32Field(object, "valueCount");

    auto AsBytes = [=,&object](auto& parser, auto defaultValue)->std::vector<std::byte>
    {
        auto value = parser(object, "valueStart", true, defaultValue);
        auto valueDelta = parser(object, "valueDelta", true, defaultValue);

        std::vector<std::byte> allBytes;
        allBytes.reserve(sizeof(value) * valueCount);
        for (size_t i = 0; i < valueCount; i++)
        {
            for (auto byte : gsl::as_bytes(gsl::make_span(&value, 1)))
            {
                allBytes.push_back(byte);
            }
            value += valueDelta;
        }
        return allBytes;
    };

    switch (dataType)
    {
    case DML_TENSOR_DATA_TYPE_FLOAT16: return AsBytes(ParseFloat16Field, half_float::half(0));
    case DML_TENSOR_DATA_TYPE_FLOAT32: return AsBytes(ParseFloat32Field, 0.0f);
    case DML_TENSOR_DATA_TYPE_FLOAT64: return AsBytes(ParseFloat64Field, 0.0);
    case DML_TENSOR_DATA_TYPE_UINT8: return AsBytes(ParseUInt8Field, static_cast<uint8_t>(0));
    case DML_TENSOR_DATA_TYPE_UINT16: return AsBytes(ParseUInt16Field, static_cast<uint16_t>(0));
    case DML_TENSOR_DATA_TYPE_UINT32: return AsBytes(ParseUInt32Field, static_cast<uint32_t>(0));
    case DML_TENSOR_DATA_TYPE_UINT64: return AsBytes(ParseUInt64Field, static_cast<uint64_t>(0));
    case DML_TENSOR_DATA_TYPE_INT8: return AsBytes(ParseInt8Field, static_cast<int8_t>(0));
    case DML_TENSOR_DATA_TYPE_INT16: return AsBytes(ParseInt16Field, static_cast<int16_t>(0));
    case DML_TENSOR_DATA_TYPE_INT32: return AsBytes(ParseInt32Field, static_cast<int32_t>(0));
    case DML_TENSOR_DATA_TYPE_INT64: return AsBytes(ParseInt64Field, static_cast<int64_t>(0));
    default: throw std::invalid_argument(fmt::format("Invalid tensor data type."));
    }
}

std::filesystem::path ResolveInputFilePath(const std::filesystem::path& parentPath, std::string_view sourcePath)
{
    auto filePathRelativeToParent = std::filesystem::absolute(parentPath / sourcePath);
    if (std::filesystem::exists(filePathRelativeToParent))
    {
        return filePathRelativeToParent;
    }

    auto filePathRelativeToCurrentDirectory = std::filesystem::absolute(sourcePath);
    return filePathRelativeToCurrentDirectory;
}

std::filesystem::path ResolveOutputFilePath(const std::filesystem::path& parentPath, std::string_view targetPath)
{
    auto filePathRelativeToParent = std::filesystem::absolute(parentPath / targetPath);
    return filePathRelativeToParent;
}

std::vector<std::byte> ReadFileContent(const std::string& fileName)
{
    std::ifstream file(fileName.c_str(), std::ifstream::ate | std::ifstream::binary);
    if (!file.is_open())
    {
        throw std::ios::failure(fmt::format("Given filename '{}' could not be opened.", fileName));
    }

    size_t fileSize = static_cast<size_t>(file.tellg());
    file.seekg(0);
    std::vector<std::byte> allBytes(fileSize);
    file.read(reinterpret_cast<char*>(allBytes.data()), fileSize);

    return allBytes;
}

std::tuple<std::vector<std::byte>, DML_TENSOR_DATA_TYPE, std::filesystem::path> GenerateInitialValuesFromFile(
    const std::filesystem::path& parentPath,
    const rapidjson::Value& object,
    const ImageTensorInfo& resampleTensorInfo,
    const std::string& resampleMode)
{
    auto sourcePath = ParseStringField(object, "sourcePath");
    auto filePath = ResolveInputFilePath(parentPath, sourcePath);

    auto fileExtension = filePath.extension().string();
    std::transform(fileExtension.begin(), fileExtension.end(), fileExtension.begin(), 
        [](unsigned char c) { return std::tolower(c); }
    );

    DML_TENSOR_DATA_TYPE tensorDataType = DML_TENSOR_DATA_TYPE_UNKNOWN;
    std::vector<std::byte> allBytes;

    if (fileExtension == ".npy")
    {
        allBytes = ReadFileContent(filePath.string());

        std::vector<uint32_t> dimensions;
        std::vector<std::byte> arrayByteData;
        ReadNpy(allBytes, /*out*/ tensorDataType, /*out*/ dimensions, /*out*/ arrayByteData);
        allBytes = std::move(arrayByteData);
    }
    else if (fileExtension == ".jpg" || fileExtension == ".png")
    {
        if (resampleMode != "scale")
        {
            // Could support cropping or other transforms in the future.
            throw std::invalid_argument("Field 'resampleMode' must be 'scale' for image files.");
        }

        allBytes = ReadTensorFromImage(filePath, resampleTensorInfo);
    }
    else
    {
        allBytes = ReadFileContent(filePath.string());
    }

    return {std::move(allBytes), tensorDataType, filePath};
}

Model::BufferDesc ParseModelBufferDesc(const std::filesystem::path& parentPath, const rapidjson::Value& object)
{
    if (!object.IsObject())
    {
        throw std::invalid_argument("Expected a non-null JSON object.");
    }

    Model::BufferDesc buffer = {};
    buffer.initialValuesDataType = ParseDmlTensorDataTypeField(object, "initialValuesDataType", /*required*/ false);

    auto ensureInitialValuesDataType = [&]()
    {
        if (buffer.initialValuesDataType == DML_TENSOR_DATA_TYPE_UNKNOWN)
        {
            throw std::invalid_argument("Field 'initialValuesDataType' is required."); 
        }
    };

    auto initialValuesField = object.FindMember("initialValues");
    if (initialValuesField == object.MemberEnd())
    {
        throw std::invalid_argument("Field 'initialValues' is required."); 
    }

    if (initialValuesField->value.IsString())
    {
        if (initialValuesField->value != "deferred")
        {
            throw std::invalid_argument("The 'initialValuesDataType' only supports deferred");
        }
        buffer.initialValues.clear();
        buffer.initialValuesOffsetInBytes = 0;
        buffer.sizeInBytes = 0;
        buffer.useDeferredBinding = true;
        return buffer;
    }
    else if (initialValuesField->value.IsArray())
    {
        const auto& arr = initialValuesField->value;
        bool maybeStructured = false;
        if (arr.Size() > 0 && arr[0].IsObject())
        {
            const auto& first = arr[0];
            maybeStructured = first.HasMember("offset") && first.HasMember("size");
        }

        if (maybeStructured)
        {
            // New structured constant buffer format: each entry supplies name,type,value,offset,size.
            // This path ignores legacy layout/sizeInBytes (size derived), and forces UNKNOWN sentinel at the buffer level.
            buffer.initialValuesDataType = DML_TENSOR_DATA_TYPE_UNKNOWN; // authoritative per-entry types
            struct PendingFieldBytes { uint64_t offset; uint64_t span; std::vector<std::byte> data; std::string name; std::string type; };
            std::vector<PendingFieldBytes> pending; pending.reserve(arr.Size());
            uint64_t maxEnd = 0;

            auto parseTypeComponentCount = [](std::string_view type)->uint32_t
            {
                // Extract trailing digits as vector length; default 1
                size_t i = type.size();
                while (i>0 && isdigit(static_cast<unsigned char>(type[i-1]))) { --i; }
                if (i == type.size()) return 1; // no digits
                uint32_t n = static_cast<uint32_t>(std::stoul(std::string(type.substr(i))));
                return n == 0 ? 1 : n;
            };
            auto parseBaseKind = [](std::string s){
                for (auto& c : s) c = static_cast<char>(::toupper(static_cast<unsigned char>(c)));
                // Strip trailing digits
                while (!s.empty() && isdigit(static_cast<unsigned char>(s.back()))) s.pop_back();
                return s;
            };
            auto writeScalar = [](std::vector<std::byte>& out, const std::string& baseKind, double value){
                if (baseKind == "FLOAT")
                {
                    float f = static_cast<float>(value);
                    std::byte* b = reinterpret_cast<std::byte*>(&f);
                    out.insert(out.end(), b, b+sizeof(f));
                }
                else if (baseKind == "UINT")
                {
                    uint32_t v = static_cast<uint32_t>(value);
                    std::byte* b = reinterpret_cast<std::byte*>(&v);
                    out.insert(out.end(), b, b+sizeof(v));
                }
                else if (baseKind == "INT")
                {
                    int32_t v = static_cast<int32_t>(value);
                    std::byte* b = reinterpret_cast<std::byte*>(&v);
                    out.insert(out.end(), b, b+sizeof(v));
                }
                else if (baseKind == "BOOL")
                {
                    uint32_t v = value != 0.0 ? 1u : 0u; // bool is 4 bytes in cbuffers
                    std::byte* b = reinterpret_cast<std::byte*>(&v);
                    out.insert(out.end(), b, b+sizeof(v));
                }
                else
                {
                    throw std::invalid_argument("Unsupported constant buffer field base type '" + baseKind + "'.");
                }
            };

            for (rapidjson::SizeType i = 0; i < arr.Size(); ++i)
            {
                const auto& entry = arr[i];
                if (!entry.IsObject())
                {
                    throw std::invalid_argument("Structured constant buffer 'initialValues' entries must all be objects.");
                }
                uint64_t offset = ParseUInt64Field(entry, "offset", true, 0);
                uint64_t span = ParseUInt64Field(entry, "size", true, 0);
                std::string name = ParseStringField(entry, "name", false, fmt::format("PADDING_{}", i));
                std::string type = ParseStringField(entry, "type", false, "");
                PendingFieldBytes pf{offset, span, {}, name, type};

                if (!type.empty())
                {
                    uint32_t componentCount = parseTypeComponentCount(type);
                    std::string baseKind = parseBaseKind(type);
                    if (entry.HasMember("value"))
                    {
                        const auto& v = entry["value"];
                        if (v.IsArray())
                        {
                            if (v.Size() != componentCount)
                            {
                                throw std::invalid_argument(fmt::format("Field '{}' expects {} components but value has {}.", name, componentCount, v.Size()));
                            }
                            for (rapidjson::SizeType j = 0; j < v.Size(); ++j)
                            {
                                if (!v[j].IsNumber()) throw std::invalid_argument("Vector component must be numeric.");
                                writeScalar(pf.data, baseKind, v[j].GetDouble());
                            }
                        }
                        else if (v.IsNumber())
                        {
                            if (componentCount != 1)
                            {
                                throw std::invalid_argument(fmt::format("Field '{}' expects {} components but scalar provided.", name, componentCount));
                            }
                            writeScalar(pf.data, baseKind, v.GetDouble());
                        }
                        else
                        {
                            throw std::invalid_argument("Field 'value' must be number or array for structured constant buffer entry.");
                        }
                    }
                }
                // If no type or value (padding), leave pf.data empty; bytes remain zero.
                if (pf.data.size() > span)
                {
                    throw std::invalid_argument(fmt::format("Field '{}' data byte size {} exceeds declared span {}.", name, pf.data.size(), span));
                }
                maxEnd = std::max<uint64_t>(maxEnd, offset + span);
                pending.emplace_back(std::move(pf));
            }

            auto align16 = [](uint64_t v){ return (v + 15ull) & ~15ull; };
            // If caller supplied sizeInBytes keep it (validated later). Otherwise derive.
            bool explicitSize = object.HasMember("sizeInBytes");
            uint64_t derivedSize = align16(maxEnd);
            if (explicitSize)
            {
                buffer.sizeInBytes = ParseUInt64Field(object, "sizeInBytes", false, derivedSize);
                if (buffer.sizeInBytes < derivedSize)
                {
                    throw std::invalid_argument(fmt::format("Provided sizeInBytes {} is smaller than derived structured constant buffer size {}.", buffer.sizeInBytes, derivedSize));
                }
            }
            else
            {
                buffer.sizeInBytes = derivedSize;
            }
            buffer.initialValuesOffsetInBytes = 0;
            buffer.initialValues.assign(buffer.sizeInBytes, std::byte{0});
            for (auto& pf : pending)
            {
                if (!pf.data.empty())
                {
                    if (pf.offset + pf.data.size() > buffer.initialValues.size())
                    {
                        throw std::invalid_argument("Internal error writing structured cbuffer bytes (overflow).");
                    }
                    std::memcpy(buffer.initialValues.data() + pf.offset, pf.data.data(), pf.data.size());
                }
                Model::BufferDesc::ConstantBufferField field{pf.name, pf.type, static_cast<uint32_t>(pf.offset), static_cast<uint32_t>(pf.span)};
                buffer.cbufferFields.push_back(std::move(field));
            }

            // Optional structType discriminator
            buffer.structType = ParseStringField(object, "structType", false, "");
            buffer.useDeferredBinding = false;
            return buffer; // structured path complete
        }

        // Legacy/flat initialValues handling
        // e.g. "initialValues": [{"type": "UINT32", "value": 42}, {"type": "FLOAT32", "value": 3.14159}]
        if (buffer.initialValuesDataType == DML_TENSOR_DATA_TYPE_UNKNOWN)
        {
            buffer.initialValues = ParseMixedPrimitiveArray(arr);
        }
        // e.g. "initialValues": [1,2,3]
        else
        {
            buffer.initialValues = GenerateInitialValuesFromList(buffer.initialValuesDataType, arr);
        }
    } 
    else if (initialValuesField->value.IsObject())
    {
        // e.g. "initialValues": { "value": 0, "valueCount": 3 }
        if (initialValuesField->value.HasMember("value"))
        {
            if (initialValuesField->value.HasMember("valueStart") || initialValuesField->value.HasMember("sourcePath"))
            {
                throw std::invalid_argument("The 'initialValuesDataType' may contain a value, valueStart, or sourcePath, but they are mutually exclusive.");
            }

            ensureInitialValuesDataType();
            buffer.initialValues = GenerateInitialValuesFromConstant(buffer.initialValuesDataType, initialValuesField->value);
        }
        // e.g. "initialValues": { "valueStart": 0, "valueDelta": 2, "valueCount": 10 }
        else if (initialValuesField->value.HasMember("valueStart"))
        {
            ensureInitialValuesDataType();
            buffer.initialValues = GenerateInitialValuesFromSequence(buffer.initialValuesDataType, initialValuesField->value);
        }
        // e.g. "initialValues": { "sourcePath": "inputFile.npy" }
        else if (initialValuesField->value.HasMember("sourcePath"))
        {
            std::vector<uint32_t> resampleSize = ParseUInt32ArrayAsVectorField(object, "resampleSize", false, {});

            if (!resampleSize.empty() && resampleSize.size() != 4)
            {
                throw std::invalid_argument("Field 'resampleSize' must be empty or have four dimensions in N,C,H,W order. N must be 1.");
            }

            std::string extension = std::filesystem::path(initialValuesField->value["sourcePath"].GetString()).extension().string();
            std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) { return std::tolower(c); });

            ImageTensorInfo dstTensorInfo = {};
            if (extension == ".jpg" || extension == ".png")
            {
                dstTensorInfo.dataType = buffer.initialValuesDataType;
                dstTensorInfo.channels = resampleSize.size() > 1 ? resampleSize[1] : 0;
                dstTensorInfo.height = resampleSize.size() > 2 ? resampleSize[2] : 0;
                dstTensorInfo.width = resampleSize.size() > 3 ? resampleSize[3] : 0;
                dstTensorInfo.sizeInBytes = GetSizeInBytes(dstTensorInfo.dataType) * dstTensorInfo.channels * dstTensorInfo.height * dstTensorInfo.width;
                dstTensorInfo.layout = ImageTensorLayout::NCHW;
                dstTensorInfo.channelOrder = dstTensorInfo.channels == 3 ? ImageTensorChannelOrder::RGB : ImageTensorChannelOrder::RGBA;
            }
            
            std::string resampleMode = ParseStringField(object, "resampleMode", false, "scale");

            auto [initialValues, fileBufferDataType, fileName] = GenerateInitialValuesFromFile(
                parentPath, 
                initialValuesField->value,
                dstTensorInfo,
                resampleMode
            );

            // Depending on the file type (.npy vs .dat), the file may have an explict data type.
            // Use the data type if present, else require initialValuesDataType if not.
            if (buffer.initialValuesDataType == DML_TENSOR_DATA_TYPE_UNKNOWN)
            {
                buffer.initialValuesDataType = fileBufferDataType;
            }

            if ((fileBufferDataType != DML_TENSOR_DATA_TYPE_UNKNOWN) && (fileBufferDataType != buffer.initialValuesDataType))
            {
                throw std::invalid_argument(fmt::format("Data type from file '{}' does not match field 'initialValuesDataType'.", fileName.string()));
            }

            ensureInitialValuesDataType(); // Raw data requires 'initialValuesDataType'. Typed data (e.g. .npy) already had a type.
            buffer.initialValues = std::move(initialValues);
        }
        else
        {
            throw std::invalid_argument("Error parsing 'initialValues' object: unknown generator type."); 
        }
    }
    else
    {
        throw std::invalid_argument("Field 'initialValues' must be an array of numbers, an object, or deferred.");
    }

    if (buffer.initialValues.empty())
    {
        throw std::invalid_argument("'initialValues' must be non-empty.");
    }

    if (buffer.cbufferFields.empty()) // skip legacy size inference if structured path already handled size
    {
        buffer.sizeInBytes = ParseUInt64Field(object, "sizeInBytes", false, buffer.initialValues.size());
        if (!object.HasMember("sizeInBytes"))
        {
            // Unless the size was explicitly set, round up to the nearest 4 bytes.
            buffer.sizeInBytes = (buffer.sizeInBytes + 3) & ~3ull;
        }
    }

    buffer.initialValuesOffsetInBytes = ParseUInt64Field(object, "initialValuesOffsetInBytes", false, 0);

    if (buffer.cbufferFields.empty()) // structured path already validated
    {
        if (buffer.initialValues.size() + buffer.initialValuesOffsetInBytes > buffer.sizeInBytes)
        {
            throw std::invalid_argument(fmt::format(
                "The buffer size ({} bytes) is too small for the initialValues ({} bytes) at offset {} bytes.", 
                buffer.sizeInBytes, 
                buffer.initialValues.size(),
                buffer.initialValuesOffsetInBytes));
        }
    }

    return buffer;
}

Model::ResourceDesc ParseModelResourceDesc(
    std::string_view name,
    const std::filesystem::path& parentPath,
    const rapidjson::Value& object)
{
    Model::ResourceDesc desc;
    desc.name = name;
    // Heuristic / explicit discriminator for resource kind.
    // If "kind" field present, honor it. Otherwise, fall back to legacy buffer parse.
    std::string kind;
    if (object.IsObject())
    {
        auto kindIt = object.FindMember("kind");
        if (kindIt != object.MemberEnd() && kindIt->value.IsString())
        {
            kind = kindIt->value.GetString();
        }
    }

    auto iequals = [](const std::string& a, const char* b){ return _stricmp(a.c_str(), b) == 0; }; 

    if (kind.empty())
    {
        // Infer texture if it has typical texture fields and no buffer-only fields.
        if (object.IsObject() && object.HasMember("width") && object.HasMember("height"))
        {
            kind = "texture";
        }
        else if (object.IsObject() && object.HasMember("filter") && object.HasMember("addressU"))
        {
            kind = "sampler";
        }
        else
        {
            kind = "buffer"; // legacy
        }
    }

    if (iequals(kind, "buffer"))
    {
        desc.value = ParseModelBufferDesc(parentPath, object);
    }
    else if (iequals(kind, "texture"))
    {
        Model::TextureDesc tex = {};
        // Required fields
        tex.width = ParseUInt32Field(object, "width", true, 0);
        tex.height = ParseUInt32Field(object, "height", true, 0);
        // Optional structural fields.
        std::string dimensionStr = ParseStringField(object, "dimension", false, "2D");
        // Depth only relevant for 3D
        bool dimensionIs3D = false;
        uint32_t mipLevels = ParseUInt32Field(object, "mipLevels", false, 1);
        uint32_t arraySize = ParseUInt32Field(object, "arraySize", false, 1); // may be overridden for cube / cube arrays
        bool isCube = false;
        bool isCubeArray = false;
        uint32_t cubeCount = 0; // logical cube count (faces = cubeCount*6)
        auto dimLower = dimensionStr; std::transform(dimLower.begin(), dimLower.end(), dimLower.begin(), ::tolower);
        if (dimLower == "2d")
        {
            if (arraySize != 1) { throw std::invalid_argument("For dimension '2D', arraySize must be exactly 1. Use '2DArray' for layered textures."); }
        }
        else if (dimLower == "2darray")
        {
            if (arraySize < 1) { throw std::invalid_argument("For dimension '2DArray', arraySize must be >= 1."); }
        }
        else if (dimLower == "3d")
        {
            dimensionIs3D = true;
            tex.depth = ParseUInt32Field(object, "depth", true, 0);
            if (tex.depth == 0) throw std::invalid_argument("3D texture requires depth >= 1.");
            if (arraySize != 1) throw std::invalid_argument("3D texture does not support arraySize > 1 in current implementation.");
            if (object.HasMember("cubeCount")) throw std::invalid_argument("cubeCount not valid for 3D textures.");
        }
        else if (dimLower == "cube")
        {
            if (tex.width != tex.height) throw std::invalid_argument("Cube texture requires width==height.");
            isCube = true; cubeCount = 1; arraySize = 6; // physical faces
        }
        else if (dimLower == "cubearray" || dimLower == "texturecubearray")
        {
            if (tex.width != tex.height) throw std::invalid_argument("CubeArray texture requires width==height.");
            isCubeArray = true;
            // cubeCount may be specified explicitly OR inferred from arraySize if multiple of 6.
            bool cubeCountSpecified = false;
            if (object.HasMember("cubeCount"))
            {
                cubeCount = ParseUInt32Field(object, "cubeCount", true, 0);
                cubeCountSpecified = true;
                if (cubeCount == 0) throw std::invalid_argument("cubeCount must be > 0 for CubeArray textures.");
                arraySize = cubeCount * 6; // override physical array size
            }
            if (!cubeCountSpecified)
            {
                // Attempt inference from arraySize
                if (arraySize % 6 != 0 || arraySize == 0)
                {
                    throw std::invalid_argument("CubeArray texture must specify either cubeCount or arraySize multiple of 6.");
                }
                cubeCount = arraySize / 6;
            }
        }
        else
        {
            throw std::invalid_argument("Unsupported texture dimension. Supported: 2D, 3D, Cube, CubeArray.");
        }
        if (mipLevels != 1) { throw std::invalid_argument("Only a single mip level is currently supported (mipLevels must be 1)."); }
        std::string usageStr = ParseStringField(object, "usage", false, "srv");
        bool allowUav = false;
        if (_stricmp(usageStr.c_str(), "srv") == 0)
        {
            allowUav = false;
        }
        else if (_stricmp(usageStr.c_str(), "uav") == 0)
        {
            allowUav = true; // RWTexture only; no SRV view automatically created. Shader reflection determines descriptor kind.
        }
        else if (_stricmp(usageStr.c_str(), "srv_uav") == 0)
        {
            allowUav = true; // Allow both descriptor types (resource created with UAV flag).
        }
        else
        {
            throw std::invalid_argument("Texture 'usage' must be one of: 'srv', 'uav', 'srv_uav'.");
        }
        // Optional format string using existing DXGI parser if present; else default RGBA8.
        if (object.HasMember("format"))
        {
            tex.format = ParseDxgiFormat(object["format"]);
        }
        else
        {
            tex.format = DXGI_FORMAT_R8G8B8A8_UNORM;
        }
        // Optional initial data: allow { "initialValues": { "sourcePath": "..." } } similar to buffers
        auto initIt = object.FindMember("initialValues");
        bool rawArrayInitialValues = false;
        if (initIt != object.MemberEnd())
        {
            if (initIt->value.IsObject() && initIt->value.HasMember("sourcePath"))
            {
                auto [fileData, fileType, fileName] = GenerateInitialValuesFromFile(parentPath, initIt->value, {/*unused*/}, "scale");
                tex.initialData = std::move(fileData);
            }
            else if (initIt->value.IsArray())
            {
                // Accept raw byte array (uint8) for simplicity.
                tex.initialData = ParseArrayAsBytes<uint8_t>(initIt->value, ParseUInt8);
                rawArrayInitialValues = true;
            }
        }
        // Validate raw array byte count matches dimensions (only for raw arrays we created directly; file loader paths may have their own semantics)
        if (rawArrayInitialValues && !tex.initialData.empty())
        {
            auto BytesPerPixel = [](DXGI_FORMAT fmt)->uint32_t
            {
                switch (fmt)
                {
                case DXGI_FORMAT_R8G8B8A8_UNORM:
                case DXGI_FORMAT_B8G8R8A8_UNORM:
                case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
                case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
                    return 4;
                default:
                    throw std::invalid_argument("Raw byte array initialization only supported for 4-byte-per-pixel RGBA formats currently.");
                }
            };
            uint64_t faceOrSliceCount = 1;
            if (isCube) faceOrSliceCount = 6;
            else if (isCubeArray) faceOrSliceCount = static_cast<uint64_t>(cubeCount) * 6ULL;
            else if (arraySize > 1) faceOrSliceCount = arraySize; // 2D array
            uint64_t depthFactor = dimensionIs3D ? tex.depth : 1ULL;
            uint64_t expectedSize = static_cast<uint64_t>(tex.width) * static_cast<uint64_t>(tex.height) * depthFactor * BytesPerPixel(tex.format) * faceOrSliceCount;
            if (tex.initialData.size() != expectedSize)
            {
                throw std::invalid_argument(fmt::format(
                    "Texture initialValues byte length ({}) does not match width*height*depth*Bpp ({}).", 
                    tex.initialData.size(), expectedSize));
            }
        }
        // Store extended metadata
        tex.mipLevels = mipLevels;
        tex.arraySize = arraySize;
        tex.isCube = isCube;
        tex.isCubeArray = isCubeArray;
        tex.cubeCount = (isCube || isCubeArray) ? cubeCount : 0u;
        if (!dimensionIs3D) tex.depth = 1; // ensure depth=1 for non-3D
    tex.allowUav = allowUav;
    desc.value = std::move(tex);
    }
    else if (iequals(kind, "sampler"))
    {
        Model::SamplerDesc samp = {};
        auto parseAddress = [](const rapidjson::Value& v)->D3D12_TEXTURE_ADDRESS_MODE
        {
            if (!v.IsString()) throw std::invalid_argument("Sampler address mode must be string");
            std::string m = v.GetString();
            if (!_stricmp(m.c_str(), "wrap")) return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
            if (!_stricmp(m.c_str(), "clamp")) return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
            if (!_stricmp(m.c_str(), "mirror")) return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
            if (!_stricmp(m.c_str(), "border")) return D3D12_TEXTURE_ADDRESS_MODE_BORDER;
            if (!_stricmp(m.c_str(), "mirror_once")) return D3D12_TEXTURE_ADDRESS_MODE_MIRROR_ONCE;
            throw std::invalid_argument("Unknown sampler address mode");
        };
        auto parseFilter = [](const rapidjson::Value& v)->D3D12_FILTER
        {
            if (!v.IsString()) throw std::invalid_argument("Sampler filter must be string");
            std::string f = v.GetString();
            if (!_stricmp(f.c_str(), "point")) return D3D12_FILTER_MIN_MAG_MIP_POINT;
            if (!_stricmp(f.c_str(), "linear")) return D3D12_FILTER_MIN_MAG_MIP_LINEAR;
            if (!_stricmp(f.c_str(), "anisotropic")) return D3D12_FILTER_ANISOTROPIC;
            return D3D12_FILTER_MIN_MAG_MIP_LINEAR; // default
        };
        if (object.HasMember("filter")) samp.filter = parseFilter(object["filter"]);
        if (object.HasMember("addressU")) samp.addressU = parseAddress(object["addressU"]);
        if (object.HasMember("addressV")) samp.addressV = parseAddress(object["addressV"]);
        if (object.HasMember("addressW")) samp.addressW = parseAddress(object["addressW"]);
        if (object.HasMember("mipLODBias")) samp.mipLODBias = ParseFloat32(object["mipLODBias"]);
        if (object.HasMember("maxAnisotropy")) samp.maxAnisotropy = ParseUInt32(object["maxAnisotropy"]);
        if (object.HasMember("comparisonFunc"))
        {
            if (!object["comparisonFunc"].IsString()) throw std::invalid_argument("comparisonFunc must be string");
            std::string cf = object["comparisonFunc"].GetString();
            if (!_stricmp(cf.c_str(), "less")) samp.comparisonFunc = D3D12_COMPARISON_FUNC_LESS;
            else if (!_stricmp(cf.c_str(), "lequal")) samp.comparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
            else if (!_stricmp(cf.c_str(), "greater")) samp.comparisonFunc = D3D12_COMPARISON_FUNC_GREATER;
            else if (!_stricmp(cf.c_str(), "gequal")) samp.comparisonFunc = D3D12_COMPARISON_FUNC_GREATER_EQUAL;
            else if (!_stricmp(cf.c_str(), "equal")) samp.comparisonFunc = D3D12_COMPARISON_FUNC_EQUAL;
            else if (!_stricmp(cf.c_str(), "notequal")) samp.comparisonFunc = D3D12_COMPARISON_FUNC_NOT_EQUAL;
            else if (!_stricmp(cf.c_str(), "never")) samp.comparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
            else if (!_stricmp(cf.c_str(), "always")) samp.comparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        }
        if (object.HasMember("borderColor") && object["borderColor"].IsArray() && object["borderColor"].GetArray().Size() == 4)
        {
            for (uint32_t i=0;i<4;i++) samp.borderColor[i] = object["borderColor"].GetArray()[i].GetFloat();
        }
        if (object.HasMember("minLOD")) samp.minLOD = ParseFloat32(object["minLOD"]);
        if (object.HasMember("maxLOD")) samp.maxLOD = ParseFloat32(object["maxLOD"]);
        // Additional sampler validation aligned with schema intents
        if (samp.filter == D3D12_FILTER_ANISOTROPIC && samp.maxAnisotropy < 2)
        {
            throw std::invalid_argument("Sampler maxAnisotropy must be >= 2 when filter is anisotropic.");
        }
        bool anyBorder = (samp.addressU == D3D12_TEXTURE_ADDRESS_MODE_BORDER) || (samp.addressV == D3D12_TEXTURE_ADDRESS_MODE_BORDER) || (samp.addressW == D3D12_TEXTURE_ADDRESS_MODE_BORDER);
        if (anyBorder && !object.HasMember("borderColor"))
        {
            throw std::invalid_argument("Sampler with border address mode requires 'borderColor' array of 4 numbers.");
        }
        desc.value = std::move(samp);
    }
    else
    {
        throw std::invalid_argument("Unknown resource kind");
    }
    return desc;
}

Model::HlslDispatchableDesc ParseModelHlslDispatchableDesc(const std::filesystem::path& parentPath, const rapidjson::Value& object)
{
    Model::HlslDispatchableDesc desc = {};

    // Compiler (required)
    auto compilerStr = ParseStringField(object, "compiler", false, "dxc");
    if (!_stricmp(compilerStr.data(), "dxc"))
    {
        desc.compiler = Model::HlslDispatchableDesc::Compiler::DXC;
    }
    else
    {
        throw std::invalid_argument("Unrecognized compiler");
    }

    // Compiler arguments model:
    //  - Compute: requires top-level compilerArgs array.
    //  - Graphics: per-stage compilerArgs arrays under graphics.vertex / graphics.pixel.

    bool hasShader = object.HasMember("shader"); 
    bool hasGraphics = object.HasMember("graphics");

    if (hasShader && hasGraphics)
    {
        throw std::invalid_argument("Dispatchable cannot define both 'shader' and 'graphics'.");
    }

    if (hasShader)
    {
        auto compilerArgsValue = object.FindMember("compilerArgs");

        // Top-level compilerArgs are required for single-shader dispatchables (schema enforces this when 'shader' present).
        if (compilerArgsValue == object.MemberEnd() || !compilerArgsValue->value.IsArray())
        {
            throw std::invalid_argument("Single-shader dispatchable requires a top-level 'compilerArgs' array.");
        }
        // Populate desc.compilerArgs from JSON before any -E/-T injection so user-supplied flags are preserved.
        for (auto& v : compilerArgsValue->value.GetArray())
        {
            if (!v.IsString())
            {
                throw std::invalid_argument("Each element of 'compilerArgs' must be a string.");
            }
            desc.compilerArgs.push_back(v.GetString());
        }

        const auto& shaderObject = object["shader"];
        if (!shaderObject.IsObject())
        {
            throw std::invalid_argument("'shader' must be an object");
        }
        // Source path
        std::string sourcePath = ParseStringField(shaderObject, "sourcePath");
        desc.sourcePath = ResolveInputFilePath(parentPath, sourcePath);
        // Entry point (default main)
        std::string entry = ParseStringField(shaderObject, "entryPoint", false, "main");

        // Determine pipeline kind strictly from entry point (do not inject -E/-T; JSON already supplies them)
        std::string upper = entry; 
        std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
        if (upper == "CS")
        {
            desc.pipelineKind = Model::HlslDispatchableDesc::PipelineKind::Compute;
        }
        else if (upper == "VS" || upper == "PS")
        {
            desc.pipelineKind = Model::HlslDispatchableDesc::PipelineKind::NonExecutable; // single VS/PS not executable
        }
        else
        {
            // Default: treat as compute if unknown.
            desc.pipelineKind = Model::HlslDispatchableDesc::PipelineKind::Compute;
        }
    }
    else if (hasGraphics)
    {
        // Guard against accidental presence of top-level compilerArgs for graphics (schema should already forbid this).
        if (object.HasMember("compilerArgs"))
        {
            throw std::invalid_argument("Top-level 'compilerArgs' is only valid for single-shader dispatchables; graphics uses per-stage compilerArgs.");
        }

        const auto& graphics = object["graphics"];
        if (!graphics.IsObject())
        {
            throw std::invalid_argument("'graphics' must be an object");
        }

        const auto& vertex = graphics["vertex"];
        if (!vertex.IsObject())
        {
            throw std::invalid_argument("graphics.vertex must be an object");
        }

        bool hasPixel = graphics.HasMember("pixel") && graphics["pixel"].IsObject();
        const rapidjson::Value* pixelPtr = hasPixel ? &graphics["pixel"] : nullptr;

        // Per-stage compilerArgs (vertex required; pixel optional)
        auto vsArgsIt = vertex.FindMember("compilerArgs");
        // For PS-only graphics dispatchables we intentionally allow omission of vertex.compilerArgs
        // and treat the resulting dispatchable as NonExecutable (compile-only reflection).
        if (vsArgsIt != vertex.MemberEnd())
        {
            if (!vsArgsIt->value.IsArray())
            {
                throw std::invalid_argument("graphics.vertex.compilerArgs must be an array when present");
            }
            for (auto& a : vsArgsIt->value.GetArray()) 
            { 
                desc.vsCompilerArgs.push_back(a.GetString()); 
            }
        }

        if (pixelPtr)
        {
            auto psArgsIt = pixelPtr->FindMember("compilerArgs");
            if (psArgsIt == pixelPtr->MemberEnd() || !psArgsIt->value.IsArray())
            {
                throw std::invalid_argument("graphics.pixel.compilerArgs must be an array when pixel stage is present");
            }
            for (auto& a : psArgsIt->value.GetArray()) 
            { 
                desc.psCompilerArgs.push_back(a.GetString()); 
            }
        }

        // Vertex stage (optional for PS-only). If sourcePath is omitted or empty, we
        // will later classify this as a NonExecutable PS-only dispatchable.
        std::string vsSource = ParseStringField(vertex, "sourcePath", false, "");
        if (!vsSource.empty())
        {
            desc.vertexShaderPath = ResolveInputFilePath(parentPath, vsSource);
            desc.vsEntryPoint = ParseStringField(vertex, "entryPoint", false, "main");
        }
        // Optional pixel stage
        if (pixelPtr)
        {
            std::string psSource = ParseStringField(*pixelPtr, "sourcePath");
            desc.pixelShaderPath = ResolveInputFilePath(parentPath, psSource);
            desc.psEntryPoint = ParseStringField(*pixelPtr, "entryPoint", false, "main");
        }

        // Formats & topology
        if (graphics.HasMember("rtvFormats"))
        {
            const auto& rtv = graphics["rtvFormats"];
            if (!rtv.IsArray())
            {
                throw std::invalid_argument("graphics.rtvFormats must be an array");
            }
            for (auto& f : rtv.GetArray())
            {
                desc.rtvFormats.push_back(ParseDxgiFormat(f));
            }
        }
        if (desc.rtvFormats.empty() && !desc.pixelShaderPath.empty())
        {
            // Default only when pixel stage exists; VS-only pipeline may have zero render targets.
            desc.rtvFormats.push_back(DXGI_FORMAT_R8G8B8A8_UNORM);
        }
        if (graphics.HasMember("dsvFormat") && graphics["dsvFormat"].IsString())
        {
            desc.dsvFormat = ParseDxgiFormat(graphics["dsvFormat"]);
        }
        if (graphics.HasMember("primitiveTopology") && graphics["primitiveTopology"].IsString())
        {
            std::string topoStr = graphics["primitiveTopology"].GetString();
            auto lower = topoStr;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            if (lower == "trianglelist")
            {
                desc.primitiveTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
            }
            else if (lower == "linelist")
            {
                desc.primitiveTopology = D3D_PRIMITIVE_TOPOLOGY_LINELIST;
            }
            else if (lower == "pointlist")
            {
                desc.primitiveTopology = D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
            }
            else
            {
                throw std::invalid_argument("Unsupported primitiveTopology (supported: trianglelist, linelist, pointlist)");
            }
        }
        if (graphics.HasMember("vertexCount") && graphics["vertexCount"].IsUint())
        {
            desc.vertexCount = graphics["vertexCount"].GetUint();
        }
        if (!desc.vertexShaderPath.empty())
        {
            if (desc.vertexCount == 0)
            {
                throw std::invalid_argument("graphics.vertexCount must be > 0 for now (indexed draws not implemented)");
            }
            // Standard graphics pipeline: VS (required) + optional PS.
            desc.pipelineKind = Model::HlslDispatchableDesc::PipelineKind::Graphics;
        }
        else
        {
            // No vertex shader path: treat as PS-only graphics dispatchable which is
            // categorized as NonExecutable. It will still be compiled for reflection,
            // but Bind/Dispatch will skip execution.
            desc.pipelineKind = Model::HlslDispatchableDesc::PipelineKind::NonExecutable;
        }
    }
    else
    {
        // Must specify either 'shader' or 'graphics'.
        throw std::invalid_argument("Dispatchable must contain either 'shader' or 'graphics' object; legacy 'compute' key is no longer supported.");
    }
    return desc;
}


Model::BufferBindingSource ParseBufferBindingSource(const rapidjson::Value& value)
{
    Model::BufferBindingSource bindingSource = {};

    if (value.IsString())
    {
        bindingSource.name = value.GetString();
    }
    else if (value.IsObject())
    {
        bindingSource.name = ParseStringField(value, "name");
        bindingSource.elementCount = ParseUInt64Field(value, "elementCount", false, 0);
        bindingSource.elementSizeInBytes = ParseUInt64Field(value, "elementSizeInBytes", false, 0);
        bindingSource.elementOffset = ParseUInt64Field(value, "elementOffset", false, 0);
        if (value.HasMember("format"))
        {
            bindingSource.format = ParseDxgiFormat(value["format"]);
        }
        if (value.HasMember("counter"))
        {
            bindingSource.counterName = ParseStringField(value, "counter");
            bindingSource.counterOffsetBytes = ParseUInt64Field(value, "counterOffsetBytes", false);
        }
        bindingSource.shape = ParseInt64ArrayAsVectorField(value, "shape", false);
        if (value.HasMember("replicate"))
        {
            if (!value["replicate"].IsBool()) throw std::invalid_argument("Field 'replicate' must be a boolean.");
            bindingSource.replicate = value["replicate"].GetBool();
        }
    }

    return bindingSource;
}

std::vector<Model::BufferBindingSource> ParseBindingSource(const rapidjson::Value& object)
{
    std::vector<Model::BufferBindingSource> sourceResources;
    if (object.IsArray())
    {
        throw std::invalid_argument("Enumerating multiple distinct resources for a single binding is no longer supported. Provide a single resource (optionally with 'replicate': true) instead.");
    }
    sourceResources.push_back(ParseBufferBindingSource(object));
    return sourceResources;
}


void ParseBindings(const rapidjson::Value& object, std::unordered_map<std::string, std::vector<Model::BufferBindingSource>>& initBindings)
{
    auto bindingsField = object.FindMember("bindings");
    if (bindingsField != object.MemberEnd() && bindingsField->value.IsObject())
    {
        for (auto bindingMember = bindingsField->value.MemberBegin(); bindingMember != bindingsField->value.MemberEnd(); bindingMember++)
        {
            initBindings[bindingMember->name.GetString()] = ParseBindingSource(bindingMember->value);
        }
    }
}


Model::DispatchableDesc ParseModelDispatchableDesc(
    std::string_view name,
    const std::filesystem::path& parentPath,
    const rapidjson::Value& object,
    BucketAllocator& allocator)
{
    if (!object.IsObject())
    {
        throw std::invalid_argument("Expected a non-null JSON object.");
    }

    Model::DispatchableDesc desc;
    desc.name = name;
    // 'type' is optional now; if provided must be "hlsl" (enforced in schema). Ignore value.
    desc.value = ParseModelHlslDispatchableDesc(parentPath, object);

    return desc;
}

Model::DispatchCommand ParseDispatchCommand(const rapidjson::Value& object)
{
    Model::DispatchCommand command = {};
    
    command.dispatchableName = ParseStringField(object, "dispatchable");

    auto threadGroupCountField = object.FindMember("threadGroupCount");
    if (threadGroupCountField != object.MemberEnd())
    {
        if (!threadGroupCountField->value.IsArray())
        {
            throw std::invalid_argument("If 'threadGroupCount' is present it must be an array with 3 integers larger than 1");
        }
        auto threadGroupCountArray = threadGroupCountField->value.GetArray();
        if (threadGroupCountArray.Size() != 3)
        {
            throw std::invalid_argument("If 'threadGroupCount' is present it must be an array with 3 integers larger than 1");
        }
        uint32_t x = threadGroupCountArray[0].GetUint();
        uint32_t y = threadGroupCountArray[1].GetUint();
        uint32_t z = threadGroupCountArray[2].GetUint();
        command.threadGroupCount = {x, y, z};
    }
    else
    {
        command.threadGroupCount = {1, 1, 1};
    }

    auto bindingsField = object.FindMember("bindings");
    if (bindingsField == object.MemberEnd() || !bindingsField->value.IsObject())
    {
        throw std::invalid_argument("Expected an object field named 'bindings'.");
    }

    for (auto bindingMember = bindingsField->value.MemberBegin(); bindingMember != bindingsField->value.MemberEnd(); bindingMember++)
    {
        command.bindings[bindingMember->name.GetString()] = ParseBindingSource(bindingMember->value);
    }

    return command;
}

Model::PrintCommand ParsePrintCommand(const rapidjson::Value& object)
{
    Model::PrintCommand command = {};
    command.resourceName = ParseStringField(object, "resource");
    command.verbose = ParseBoolField(object, "verbose", false, false);
    return command;
}

Model::WriteFileCommand ParseWriteFileCommand(const rapidjson::Value& object, const std::filesystem::path& outputPath)
{
    Model::WriteFileCommand command = {};
    command.resourceName = ParseStringField(object, "resource");
    command.targetPath = ResolveOutputFilePath(outputPath, ParseStringField(object, "targetPath")).string();
    BucketAllocator allocator;
    auto dimensions = ParseUInt32ArrayField(object, "dimensions", allocator, false);
    command.dimensions.assign(dimensions.begin(), dimensions.end());

    return command;
}

Model::Command ParseModelCommand(const rapidjson::Value& object, const std::filesystem::path& outputPath)
{
    return ParseModelCommandDesc(object, outputPath).command;
}

Model::CommandDesc ParseModelCommandDesc(const rapidjson::Value& object, const std::filesystem::path& outputPath)
{
    Model::CommandDesc commandDesc = {};

    commandDesc.type = ParseStringField(object, "type");
    commandDesc.parameters = RapidJsonToString(object);

    if (!_stricmp(commandDesc.type.data(), "dispatch"))
    { 
        commandDesc.command = ParseDispatchCommand(object);
    }
    else if (!_stricmp(commandDesc.type.data(), "print"))
    {
        commandDesc.command = ParsePrintCommand(object);
    }
    else if (!_stricmp(commandDesc.type.data(), "writeFile"))
    {
        commandDesc.command = ParseWriteFileCommand(object, outputPath);
    }
    else if (!_stricmp(commandDesc.type.data(), "resolveGpuTime"))
    {
        commandDesc.command = Model::ResolveGpuTimeCommand{};
    }
    else
    {
        throw std::invalid_argument("Unrecognized command");
    }

    return commandDesc;
}

// Determine the line and column in text by counting the line breaking characters
// up to the given offset. Note the line and column counts are zero-based, and so the
// caller may want to add 1 when displaying it, as most text editors show one-based
// values to the user.
void MapCharacterOffsetToLineColumn(
    std::string_view documentText,
    size_t errorOffset,
    _Out_ uint32_t& line,
    _Out_ uint32_t& column
    )
{
    uint32_t lineCount = 0;
    uint32_t columnCount = 0;

    bool precededByCr = false;
    for (size_t i = 0; i < errorOffset; ++i)
    {
        wchar_t ch = documentText[i];
        bool foundNewLine = false;

        switch (ch)
        {
        case 0x2028: // U+2028 LINE SEPARATOR
        case 0x2029: // U+2029 PARAGRAPH SEPARATOR
        case 0x000B: // U+000B VERTICAL TABULATION
        case 0x000C: // U+000C FORM FEED
        case 0x000D: // U+000D CARRIAGE RETURN
            foundNewLine = true;
            break;

        case 0x000A: // U+000A LINE FEED
            // Count CR LF pair as one line break.
            foundNewLine = !precededByCr;
            break;

        default: // Any other character.
            ++columnCount;
            break;
        }
        precededByCr = (ch == 0x000D);

        if (foundNewLine)
        {
            ++lineCount;
            columnCount = 0;
        }
    }

    line = lineCount;
    column = columnCount;
}

std::string GetJsonParseErrorMessage(
    const rapidjson::Document& jsonDocument,
    std::string_view jsonDocumentText
    )
{
    // Gather a snippet of preview text at the error, stripping any new lines for preview sake.
    // Note RapidJSON doesn't include the line number, just document offset.
    std::string_view applicableText = jsonDocumentText.substr(jsonDocument.GetErrorOffset(), 40);
    std::string newLineStrippedText(applicableText);

    for (auto& ch : newLineStrippedText)
    {
        if (ch == '\r' || ch == '\n')
            ch = ' ';
    }

    uint32_t line = 0, column = 0;
    MapCharacterOffsetToLineColumn(jsonDocumentText, jsonDocument.GetErrorOffset(), /*out*/ line, /*out*/ column);

    std::string formattedErrorMessage = fmt::format(
        "JSON parse error at char offset:{}, line:{}, column:{}, error:{} {}\nSnippet: >>>{}<<<",
        int(jsonDocument.GetErrorOffset()),
        line + 1,
        column + 1,
        int(jsonDocument.GetParseError()),
        rapidjson::GetParseError_En(jsonDocument.GetParseError()),
        newLineStrippedText.c_str()
    );

    return formattedErrorMessage;
}

Model ParseModel(
    const rapidjson::Document& doc,
    const std::string_view& jsonDocumentText,
    const std::filesystem::path& inputPath,
    const std::filesystem::path& outputPath)
{
    if (doc.HasParseError())
    {
        std::string errorMessage = GetJsonParseErrorMessage(doc, jsonDocumentText);
        throw std::invalid_argument(errorMessage);
    }

    BucketAllocator allocator;

    std::vector<Model::ResourceDesc> resources;
    auto resourcesField = doc.FindMember("resources");
    if (resourcesField != doc.MemberEnd())
    {
        if (!resourcesField->value.IsObject())
        {
            throw std::invalid_argument("If present, 'resources' must be an object");
        }
        for (auto field = resourcesField->value.MemberBegin(); field != resourcesField->value.MemberEnd(); field++)
        {
            try
            {
                resources.emplace_back(std::move(ParseModelResourceDesc(field->name.GetString(), inputPath, field->value)));
            }
            catch (std::exception& e)
            {
                throw std::invalid_argument(fmt::format("Failed to parse resource {}: {}", field->name.GetString(), e.what()));
            }
        }
    }

    std::vector<Model::DispatchableDesc> operators;
    auto dispatchablesField = doc.FindMember("dispatchables");
    if (dispatchablesField == doc.MemberEnd() || !dispatchablesField->value.IsObject())
    {
        throw std::invalid_argument("Expected an object named 'dispatchables'");
    }
    for (auto field = dispatchablesField->value.MemberBegin(); field != dispatchablesField->value.MemberEnd(); field++)
    {
        try
        {
            operators.emplace_back(std::move(ParseModelDispatchableDesc(field->name.GetString(), inputPath, field->value, allocator)));
        }
        catch (std::exception& e)
        {
            throw std::invalid_argument(fmt::format("Failed to parse dispatchable {}: {}", field->name.GetString(), e.what()));
        }
    }

    std::vector<Model::CommandDesc> commands;
    auto commandsField = doc.FindMember("commands");
    if (commandsField == doc.MemberEnd() || !commandsField->value.IsArray())
    {
        throw std::invalid_argument("Expected an array field named 'commands'");
    }
    auto commandsArray = commandsField->value.GetArray();
    for (uint32_t i = 0; i < commandsArray.Size(); i++)
    {
        try
        {
            commands.emplace_back(std::move(ParseModelCommandDesc(commandsArray[i], outputPath)));
        }
        catch (std::exception& e)
        {
            throw std::invalid_argument(fmt::format("Failed to parse command at index {}: {}", i, e.what()));
        }
    }

    return {std::move(resources), std::move(operators), std::move(commands), std::move(allocator)};
}

Model ParseModel(
    const std::filesystem::path& filePath,
    std::filesystem::path inputPath,
    std::filesystem::path outputPath)
{
    std::filesystem::path modelPath = filePath;
    if (!std::filesystem::exists(filePath))
    {
        modelPath = inputPath / filePath;
        if (!std::filesystem::exists(modelPath))
        {
            throw std::invalid_argument(fmt::format("Model does not exist. Path given: '{}'.", filePath.string()));
        }
    }
    if (std::filesystem::is_directory(modelPath))
    {
        throw std::invalid_argument(fmt::format("Model must be a JSON file, not a directory. Path given: '{}'", modelPath.string()));
    }

    std::vector<std::byte> allBytes = ReadFileContent(modelPath.string());
    allBytes.push_back(std::byte(0)); // Ensure null terminated for parser.
    char* fileContentBegin = reinterpret_cast<char*>(allBytes.data());
    std::string_view fileContent{fileContentBegin, allBytes.size()};

    rapidjson::Document doc;

    constexpr rapidjson::ParseFlag parseFlags = rapidjson::ParseFlag(
        rapidjson::kParseFullPrecisionFlag | 
        rapidjson::kParseCommentsFlag |
        rapidjson::kParseTrailingCommasFlag |
        rapidjson::kParseStopWhenDoneFlag);

    doc.ParseInsitu<parseFlags>(fileContentBegin);

    return ParseModel(doc, fileContent, inputPath, outputPath);
}

} // namespace JsonParsers