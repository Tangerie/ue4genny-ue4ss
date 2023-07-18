#pragma once

#include <string>
#include <Mod/CppUserModBase.hpp>
#include <DynamicOutput/DynamicOutput.hpp>
#include <Unreal/UObjectGlobals.hpp>
#include <Unreal/UObject.hpp>
#include <Unreal/Hooks.hpp>
#include <Unreal/UClass.hpp>
#include <Unreal/UFunction.hpp>
#include <Unreal/Common.hpp>
#include <Unreal/UFunctionStructs.hpp>
#include <Unreal/FFrame.hpp>
#include <Unreal/FString.hpp>
#include <Unreal/FText.hpp>
#include <Unreal/FProperty.hpp>
#include <Unreal/UEnum.hpp>
#pragma warning(disable: 4005)
#include <Unreal/TypeChecker.hpp>
#include <Unreal/UObjectGlobals.hpp>
#include <Unreal/UFunction.hpp>
#include <Unreal/FProperty.hpp>
#include <Unreal/UClass.hpp>
#include <Unreal/UInterface.hpp>
#include <Unreal/UScriptStruct.hpp>
#include <Unreal/UEnum.hpp>
#include <Unreal/Property/FClassProperty.hpp>
#include <Unreal/Property/FStructProperty.hpp>
#include <Unreal/Property/FArrayProperty.hpp>
#include <Unreal/Property/FMapProperty.hpp>
#include <Unreal/Property/FDelegateProperty.hpp>
#include <Unreal/Property/FMulticastInlineDelegateProperty.hpp>
#include <Unreal/Property/FMulticastSparseDelegateProperty.hpp>

using namespace RC;
using namespace RC::Unreal;

std::string narrow(const FString& fstr);
std::string narrow(const FName& fname);
std::string narrow(const std::wstring& wstr);

template <typename T, typename T2 = UObject*> T DCast(T2 In) {
    if (In && ((UObject*)In)->IsA<std::remove_pointer<T>::type>()) {
        return (T)In;
    }

    return nullptr;
}

