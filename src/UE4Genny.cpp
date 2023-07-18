#include <fstream>
#include <sstream>
#include <thread>
#include <unordered_map>

#include <Windows.h>

#include "Genny.hpp"
#include "UE4Genny.hpp"
#include "kanan/core/Scan.hpp"
#include "kanan/core/String.hpp"
#include "kanan/core/Utility.hpp"
#include <Unreal/Property/NumericPropertyTypes.hpp>
#include <Unreal/Property/FBoolProperty.hpp>
#include <Unreal/Property/FNameProperty.hpp>
#include <Unreal/Property/FEnumProperty.hpp>
#include <Unreal/Property/FStrProperty.hpp>

std::string get_full_name(UObject* obj) {
    return narrow(obj->GetFullName());
    auto c = obj->GetClass();
}

std::string narrow(const FString& fstr) {
    auto char_array = fstr.GetCharArray();
    return narrow(char_array);
}

std::string narrow(const FName& fname) {
    return narrow(fname.ToString());
}

std::string narrow(const std::wstring& wstr) {
    return to_string(wstr);
}

void generate_uenum(genny::Namespace* g, UEnum* uenum) {
    auto enum_name = narrow(uenum->GetNamePrivate());
    enum_name = enum_name.substr(enum_name.find_last_of(':') + 1);
    genny::Enum* genny_enum{};

    auto enum_class = uenum->GetClass();
    auto enum_class_name = narrow(enum_class->GetNamePrivate());

    switch (uenum->GetCppForm()) {
    case UEnum::ECppForm::EnumClass:
        genny_enum = g->enum_class(enum_name);
        break;
    case UEnum::ECppForm::Namespaced: {
        auto ns_name = enum_name;
        enum_name = narrow(uenum->GetCppType());
        enum_name = enum_name.substr(enum_name.find_last_of(':') + 1);

        if (enum_name.empty()) {
            enum_name = "Type";
        }

        genny_enum = g->namespace_(ns_name)->enum_(enum_name);

        // Unreal included types
        if (ns_name == "ESearchCase" || ns_name == "ESearchDir")
            genny_enum->skip_generation(true);

        break;
    }
    default:
        genny_enum = g->enum_(enum_name);
        break;
    }

    // NOTE: Must make UEnum::Names public.
    for (auto n = 0; n < uenum->GetNames().Num(); ++n) {
        auto& name = uenum->GetNames()[n];
        auto key = narrow(name.Key);
        key = key.substr(key.find_last_of(':') + 1);

        // Work around windows defines PF_MAX AF_INET
        if (key == "PF_MAX")
            key = "PF_MAX_";

        genny_enum->value(key, name.Value);
    }
}

std::string get_fproperty_typename(FProperty* fprop) {
    if (fprop->IsA<FByteProperty>()) {
        auto fbyte = (FByteProperty*)fprop;

        // TODO: Handle FByteProperty's that are actual enums.
        if (fbyte->GetEnum() != nullptr) {
            auto uenum = fbyte->GetEnum();
            auto enum_name = narrow(uenum->GetNamePrivate());
            enum_name = enum_name.substr(enum_name.find_last_of(':') + 1);

            if (uenum->GetCppForm()  == UEnum::ECppForm::Namespaced) {
                auto ns_name = enum_name;
                enum_name = narrow(uenum->GetCppType());
                enum_name = enum_name.substr(enum_name.find_last_of(':') + 1);

                if (enum_name.empty()) {
                    enum_name = "Type";
                }

                return ns_name + "::" + enum_name;
            } else {
                return enum_name;
            }
        } else {
            return "uint8_t";
        }
    } else if (fprop->IsA<FInt8Property>()) {
        return "int8_t";
    } else if (fprop->IsA<FInt16Property>()) {
        return "int16_t";
    } else if (fprop->IsA<FIntProperty>()) {
        return "int32_t";
    } else if (fprop->IsA<FInt64Property>()) {
        return "int64_t";
    } else if (fprop->IsA<FUInt16Property>()) {
        return "uint16_t";
    } else if (fprop->IsA<FUInt32Property>()) {
        return "uint32_t";
    } else if (fprop->IsA<FUInt64Property>()) {
        return "uint64_t";
    } else if (fprop->IsA<FFloatProperty>()) {
        return "float";
    } else if (fprop->IsA<FDoubleProperty>()) {
        return "double";
    } else if (fprop->IsA<FBoolProperty>()) {
        auto fbool = (FBoolProperty*)fprop;

        // A FieldMask of 0xFF indicates a native bool, otherwise its part of a bitfield.
        // NOTE: Must make FieldMask public.
        if (fbool->GetFieldMask() == 0xFF) {
            return "bool";
        } else {
            return "uint8_t";
        }
    } else if (fprop->IsA<FObjectProperty>()) {
        auto fobj = (FObjectProperty*)fprop;
        if (auto uclass = fobj->GetPropertyClass()) {
            return kanan::narrow(uclass->GetPrefixCPP()) + narrow(uclass->GetNamePrivate()) + '*';
        }
    } else if (fprop->IsA<FStructProperty>()) {
        auto fstruct = (FStructProperty*)fprop;
        auto ustruct = fstruct->GetStruct();
        return kanan::narrow(ustruct->GetPrefixCPP()) + narrow(ustruct->GetNamePrivate());
    } else if (fprop->IsA<FArrayProperty>()) {
        auto farray = (FArrayProperty*)fprop;
        auto inner = farray->GetInner();
        auto inner_typename = get_fproperty_typename(inner);

        if (inner_typename.empty()) {
            inner_typename = "void*";
        }

        return "TArray<" + inner_typename + '>';
    } else if (fprop->IsA<FNameProperty>()) {
        return "FName";
    } else if (fprop->IsA<FStrProperty>()) {
        return "FString";
    } else if (fprop->IsA<FEnumProperty>()) {
        auto fenum = (FEnumProperty*)fprop;
        auto uenum = fenum->GetEnum();
        auto enum_name = narrow(uenum->GetNamePrivate());
        enum_name = enum_name.substr(enum_name.find_last_of(':') + 1);

        if (uenum->GetCppForm() == UEnum::ECppForm::Namespaced) {
            auto ns_name = enum_name;
            enum_name = narrow(uenum->GetCppType());
            enum_name = enum_name.substr(enum_name.find_last_of(':') + 1);

            if (enum_name.empty()) {
                enum_name = "Type";
            }

            return ns_name + "::" + enum_name;
        } else {
            return enum_name;
        }
    }

    return "";
}

genny::Type* genny_type_for_fproperty(genny::Namespace* g, FProperty* fprop) {
    auto prop_typename = get_fproperty_typename(fprop);
    genny::Type* prop_type{};
    auto ns = g;

    while (true) {
        if (auto first_colon = prop_typename.find_first_of(':'); first_colon != std::string::npos) {
            auto ns_name = prop_typename.substr(0, first_colon);
            prop_typename = prop_typename.substr(first_colon + 2);
            ns = ns->namespace_(ns_name);
        } else {
            break;
        }
    }

    if (fprop->IsA<FByteProperty>()) {
        auto fbyte = (FByteProperty*)fprop;

        // Change the enum type to a uint8_t if necessary.
        if (fbyte->GetEnum() != nullptr) {
            auto enum_type = ns->enum_(prop_typename);
            enum_type->type(g->type("uint8_t"));
        }

        prop_type = ns->type(prop_typename);
    } else if (fprop->IsA<FArrayProperty>()) {
        auto farray = (FArrayProperty*)fprop;
        auto inner = farray->GetInner();
        auto inner_type = genny_type_for_fproperty(g, inner);

        if (inner_type != nullptr) {
            prop_type = ns->generic_type(prop_typename)->template_type(inner_type)->size(sizeof(TArray<int>));
        } else {
            prop_type = ns->generic_type(prop_typename)->size(sizeof(TArray<int>));
        }
    } else if (fprop->IsA<FEnumProperty>()) {
        auto fenum = (FEnumProperty*)fprop;

        // Change the enum type to the correct one.
        auto genny_enum = ns->enum_(prop_typename);
        auto genny_enum_type = genny_type_for_fproperty(g, fenum->GetUnderlyingProperty());

        genny_enum->type(genny_enum_type);
        prop_type = genny_enum;
    } else if (!prop_typename.empty()) {
        if (prop_typename.back() == '*') {
            prop_type = ns->type(prop_typename.substr(0, prop_typename.length() - 1))->ptr();
        } else {
            prop_type = ns->type(prop_typename);
        }
    }

    return prop_type;
}

void generate_fproperty(genny::Struct* s, FProperty* fprop) {
    auto ns = s->owner<genny::Namespace>();
    auto prop_name = narrow(fprop->GetName());
    auto prop_type = genny_type_for_fproperty(ns, fprop);

    if (fprop->IsA<FBoolProperty>()) {
        auto fbool = (FBoolProperty*)fprop;

        // A FieldMask of 0xFF indicates a native bool, otherwise its part of a bitfield.
        if (fbool->GetFieldMask() != 0xFF) {
            auto bf = s->variable(prop_name)->offset(fprop->GetOffset_ForInternal());

            auto mask = fbool->GetByteMask();
            auto offset = -1;

            for (; mask != 0; mask /= 2, ++offset);

            bf->type(prop_type);
            bf->bit_offset(offset);
            bf->bit_size(fbool->GetFieldSize());
            // bf->field(prop_name)->size(fbool->FieldSize)->offset(offset);

            // Return early since we're done for bitfields.
            return;
        }
    }

    if (prop_type != nullptr) {
        if (fprop->GetArrayDim() > 1) {
            s->variable(prop_name)
                ->type(prop_type->array_(fprop->GetArrayDim()))
                ->offset(fprop->GetOffset_ForInternal());
        } else {
            s->variable(prop_name)->type(prop_type)->offset(fprop->GetOffset_ForInternal());
        }
    } /*else {
        // Don't know what type it is so just fill up the space.
        s->array_(prop_name)
            ->count(fprop->ElementSize * fprop->ArrayDim)
            ->offset(fprop->Offset_Internal)
            ->type("uint8_t");
    }*/
}

void generate_ufunction(genny::Struct* s, UFunction* ufunc) {
    auto ns = s->owner<genny::Namespace>();
    auto func_name = narrow(ufunc->GetNamePrivate());
    genny::Function* genny_func{};

    if ((ufunc->GetFunctionFlags() & FUNC_Static) != 0) {
        genny_func = s->static_function(func_name);
    } else {
        genny_func = s->function(func_name);
    }

    auto param_struct = std::make_unique<genny::Struct>("Params_" + func_name);
    genny::Variable* ret_param{};
    std::unordered_set<genny::Variable*> out_params{};

    // Add params.
    for (auto field = ufunc->GetChildProperties(); field != nullptr; field = field->GetNextFieldAsProperty()) {
        if (field->IsA<FProperty>()) {
            auto fparam = (FProperty*)field;
            auto param_flags = fparam->GetPropertyFlags();
            auto param_name = narrow(fparam->GetName());
            auto param_type = genny_type_for_fproperty(ns, fparam);

            // Unknown parameter type (probably a soft object ptr or something we don't support) so just return early.
            if (param_type == nullptr) {
                return;
                // param_type = ns->type("uint8_t");
            }

            if ((param_flags & CPF_ReturnParm) != 0) {
                genny_func->returns(param_type);
            } else {
                auto param = genny_func->param(param_name);

                if ((param_flags & CPF_ReferenceParm) != 0 || (param_flags & CPF_OutParm) != 0) {
                    param->type(param_type->ref());
                } else {
                    param->type(param_type);
                }
            }

            auto params_param = param_struct->variable(param_name)->offset(fparam->GetOffset_ForInternal());

            if (param_type->size() == fparam->GetElementSize() * fparam->GetArrayDim()) {
                params_param->type(param_type);
            } else {
                params_param->type(ns->type("uint8_t"));
            }

            if ((param_flags & CPF_ReturnParm) != 0) {
                ret_param = params_param;
            } else if ((param_flags & CPF_ReferenceParm) != 0 || (param_flags & CPF_OutParm) != 0) {
                out_params.emplace(params_param);
            }
        }
    }

    // Generate the procedure.
    std::ostringstream os{};
#ifdef SDK_UOBJECT_STRING_LOOKUP
    os << "static auto func = (UFunction*)(find_uobject(\"" << get_full_name(ufunc) << "\"));\n";
#else
    os << "static auto func = (UFunction*)(find_uobject(" << kanan::hash(get_full_name(ufunc)) << ")); // " << get_full_name(ufunc) << "\n";
#endif
    param_struct->generate(os);
    param_struct->generate_typename_for(os, nullptr);
    os << " params{};\n";

    for (auto&& param : param_struct->get_all<genny::Variable>()) {
        // Skip return param.
        if (param == ret_param) {
            continue;
        }

        os << "params." << param->usable_name() << " = (";
        param->type()->generate_typename_for(os, nullptr);
        os << ")" << param->usable_name() << ";\n";
    }

    if (genny_func->is_a<genny::StaticFunction>()) {
        os << "((UClass*)StaticClass())->ClassDefaultObject->ProcessEvent(func, &params);\n";
    } else {
        os << "ProcessEvent(func, &params);\n";
    }

    for (auto&& param : out_params) {
        os << param->usable_name() << " = params." << param->usable_name() << ";\n";
    }

    if (ret_param != nullptr) {
        os << "return (";
        genny_func->returns()->generate_typename_for(os, nullptr);
        os << ")params." << ret_param->usable_name() << ";\n";
    }

    genny_func->procedure(os.str());
    genny_func->depends_on(ns->type("UFunction"));
}

void generate_ustruct(genny::Struct* genny_struct, UStruct* ustruct) {
    auto ustruct_name = [](UStruct* ustruct) {
        return kanan::narrow(ustruct->GetPrefixCPP()) + narrow(ustruct->GetNamePrivate());
    };
    auto ns = genny_struct->owner<genny::Namespace>();

    // Set inheritance.
    auto uparent = ustruct->GetSuperStruct();

    if (uparent != nullptr) {
        auto uparent_name = ustruct_name(uparent);

        if (uparent->IsA<UClass>()) {
            genny_struct->parent(ns->class_(uparent_name));
        } else {
            genny_struct->parent(ns->struct_(uparent_name));
        }
    }

    // Set size.
    if (ustruct->IsA<UScriptStruct>()) {
        auto uscript = (UScriptStruct*)ustruct;

        if (auto struct_ops = uscript->GetCppStructOps()) {
            genny_struct->size(struct_ops->GetSize());
        }
    }

    if (ustruct->GetPropertiesSize() > genny_struct->size()) {
        genny_struct->size(((ustruct->GetPropertiesSize() + ustruct->GetMinAlignment() - 1) / ustruct->GetMinAlignment()) *
                           ustruct->GetMinAlignment());
    }
}

void generate_ustruct_members(genny::Struct* genny_struct, UStruct* ustruct) {
    // Add properties.
    for (auto field = ustruct->GetChildProperties(); field != nullptr; field = field->GetNextFieldAsProperty()) {
        if (field->IsA<FProperty>()) {
            generate_fproperty(genny_struct, (FProperty*)field);
        }
    }
}

void generate_uclass_functions(genny::Struct* genny_struct, UClass* uclass) {
    // Add StaticClass().
    auto static_class = genny_struct->static_function("StaticClass")->returns(genny_struct->ptr());
    std::ostringstream os{};
#ifdef SDK_UOBJECT_STRING_LOOKUP
    os << "static auto res = find_uobject(\"" << get_full_name(uclass) << "\");\n";
#else
    os << "static auto res = find_uobject(" << kanan::hash(get_full_name(uclass)) << "); // " << get_full_name(uclass) << "\n";
#endif
    os << "return (";
    static_class->returns()->generate_typename_for(os, genny_struct);
    os << ")res;";
    static_class->procedure(os.str());

    // Add functions.
    for (auto field = uclass->GetChildren(); field != nullptr; field = field->GetNext()) {
        if (field->IsA<UFunction>()) {
            generate_ufunction(genny_struct, (UFunction*)field);
        }
    }
}

void generate() {
    genny::Sdk sdk{};
    auto g = sdk.global_ns();

    sdk.include("cstdint");

#ifdef SDK_ADDITIONAL_INCLUDE_HPP
    sdk.include(SDK_ADDITIONAL_INCLUDE_HPP);
#endif

    // Add basic types
    g->type("int8_t")->size(1);
    g->type("int16_t")->size(2);
    g->type("int32_t")->size(4);
    g->type("int64_t")->size(8);
    g->type("uint8_t")->size(1);
    g->type("uint16_t")->size(2);
    g->type("uint32_t")->size(4);
    g->type("uint64_t")->size(8);
    g->type("float")->size(4);
    g->type("double")->size(8);
    g->type("bool")->size(1);
    g->type("char")->size(1);
    g->type("int")->size(4);
    g->type("void")->size(0);

    // Add UE4 types
    auto uobj = g->class_("UObject");

    auto ufunc = g->class_("UFunction");
    //auto uobj_process_event = uobj->virtual_function("ProcessEvent")->vtable_index(UOBJECT_PROCESSEVENT_INDEX);
    //uobj_process_event->param("Function")->type(ufunc->ptr());
    //uobj_process_event->param("Parms")->type(g->type("void")->ptr());

    /*
    auto ustruct = g->class_("UStruct");
    ustruct->variable("SuperStruct")
        ->type(ustruct->ptr())
        ->offset(offsetof(UStruct, SuperStruct)); // NOTE: Make SuperStruct public.

    auto uclass = g->class_("UClass");
    uclass->variable("ClassDefaultObject")->type(uobj->ptr())->offset(offsetof(UClass, ClassDefaultObject));

    uobj->variable("ClassPrivate")
        ->type(uclass->ptr())
        ->offset(offsetof(UObject, ClassPrivate)); // NOTE: Make ClassPrivate public.
    */
    g->type("FName")->size(sizeof(FName));
    g->type("FString")->size(sizeof(FString));

    std::unordered_map<genny::Struct*, UStruct*> struct_map{};
    UObjectGlobals::ForEachUObject(
        [&](void* untyped_object, [[maybe_unused]] int32_t chunk_index, [[maybe_unused]] int32_t object_index) { 
            auto obj_item = untyped_object;

            if (obj_item == nullptr) {
                return LoopAction::Continue;
            }

            UObject* obj = static_cast<UObject*>(untyped_object);

            if (auto uenum = Cast<UEnum>(obj)) {
                generate_uenum(g, uenum);
            } else if (auto ustruct = Cast<UStruct>(obj)) {
                // Skip functions and blueprints for now.
                if (ustruct->IsA<UFunction>() /*|| ustruct->IsA<UBlueprintGeneratedClass>()*/) {
                    return LoopAction::Continue;
                }

                auto name = kanan::narrow(ustruct->GetPrefixCPP()) + narrow(ustruct->GetNamePrivate());
                genny::Struct* genny_struct{};

                if (ustruct->IsA<UClass>()) {
                    genny_struct = g->class_(name);
                } else {
                    genny_struct = g->struct_(name);
                }

                if (name == "FTimespan" || name == "FFloatInterval" || name == "FInt32Interval")
                    genny_struct->skip_generation(true);

                generate_ustruct(genny_struct, ustruct);
                struct_map.emplace(genny_struct, ustruct);
            }
            
            return LoopAction::Continue;
        });

    for (auto&& [genny_struct, ustruct] : struct_map) {
        generate_ustruct_members(genny_struct, ustruct);
    }

    // Have to generate the functions seperately so the size of all the types are known.
    for (auto&& [genny_struct, ustruct] : struct_map) {
        if (!ustruct->IsA<UClass>()) {
            continue;
        }

        generate_uclass_functions(genny_struct, (UClass*)ustruct);
    }

    sdk.generate(SDK_OUTPUT_DIR);
}

void startup_thread() {
    generate();
    MessageBox(nullptr, L"SDK generation finished.", L"UE4Genny", MB_ICONINFORMATION);
}

BOOL WINAPI DllMain(HINSTANCE dllHandle, DWORD reason, void* reserved) {
    if (reason == DLL_PROCESS_ATTACH) {
        std::thread{startup_thread}.detach();
    }

    return 1;
}