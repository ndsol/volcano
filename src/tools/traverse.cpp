/* Copyright (c) 2017-2018 the Volcano Authors. Licensed under the GPLv3.
 */
#include <StandAlone/ResourceLimits.h>
#include <StandAlone/Worklist.h>
#include <glslang/Include/ShHandle.h>
#include <glslang/Include/revision.h>
#include <glslang/Public/ShaderLang.h>
#include <glslang/MachineIndependent/localintermediate.h>
#include <SPIRV/GlslangToSpv.h>
#include <SPIRV/GLSL.std.450.h>
#include <SPIRV/doc.h>
#include <SPIRV/disassemble.h>
#include <stdarg.h>
#include <array>

#ifdef _MSC_VER
#define VOLCANO_PRINTF(y, z)
#else
#define VOLCANO_PRINTF(y, z) __attribute__((format(printf, y, z)))
#endif

extern const char* binaryFileName;
extern const char* entryPointName;
extern const char* sourceEntryPointName;
extern const char* shaderStageName;
extern const char* variableName;

extern std::array<unsigned int, EShLangCount> baseSamplerBinding;
extern std::array<unsigned int, EShLangCount> baseTextureBinding;
extern std::array<unsigned int, EShLangCount> baseImageBinding;
extern std::array<unsigned int, EShLangCount> baseUboBinding;
extern std::array<unsigned int, EShLangCount> baseSsboBinding;

using namespace glslang;

#define SPECIALIZATION_CONSTANTS_STRUCT_NAME "SpecializationConstants"
enum HeaderTraverseMode {
    ModeCpp,  // Output fields a c++, skipping uniforms.
    ModeAttributes,  // Output VkVertexInputAttributeDescription.
    ModeUniforms,  // Output uniform fields as c++.
    ModeSpecialization,  // Output specialization constants as c++.
    ModeSpecializationMap,  // Specialization constants as VkSpecializationMapEntry
};

class TypeToCpp {
public:
    TypeToCpp(const TType& type, const TString& fieldName, TString& structName, TString autoPrefix,
              int indent, HeaderTraverseMode mode)
        : indent(indent)
        , type(type)
        , fieldName(fieldName)
        , structName(structName)
        , autoPrefix(autoPrefix)
        , mode(mode) {}
    ~TypeToCpp() {}

    TString toCpp();

protected:
    TString toVertexInputAttributes();
    TString toSpecializationMap();
    void makeFormat(TString& format, int bits, const char* suffix);
    bool typeNeedsCustomGLM();

    const int indent;
    const TType& type;
    TString fieldName;
    TString structName;
    TString autoPrefix;
    HeaderTraverseMode mode;
};

class HeaderOutputTraverser : public TIntermTraverser {
public:
    HeaderOutputTraverser(std::string& unitFileName, HeaderTraverseMode mode)
        : sourceFile(unitFileName), mode(mode) {}

    virtual bool visitBinary(TVisit, TIntermBinary* node) {
        return false;
    }
    virtual bool visitUnary(TVisit, TIntermUnary* node) {
        return false;
    }
    virtual bool visitAggregate(TVisit, TIntermAggregate* node) {
        switch (node->getOp()) {
        case EOpSequence:
            return true;
        case EOpLinkerObjects:
            return true;
        default:
            break;
        }
        return false;
    }
    virtual bool visitSelection(TVisit, TIntermSelection* node) {
        return false;
    }
    virtual void visitConstantUnion(TIntermConstantUnion* node) {
    }
    virtual void visitSymbol(TIntermSymbol* node) {
        auto* t = node->getAsTyped();
        if (!t) {
            return;
        }
        int indent = 0;
        switch (mode) {
        case ModeCpp:
            indent = 1;
            break;
        case ModeAttributes:
            indent = 2;
            break;
        case ModeUniforms:
            indent = 0;
            break;
        case ModeSpecialization:
            indent = 1;
            break;
        case ModeSpecializationMap:
            indent = 2;
            break;
        }
        TypeToCpp converter(t->getType(), node->getName(), structName, "", indent, mode);
        out.debug << converter.toCpp();
    }
    virtual bool visitLoop(TVisit, TIntermLoop* node) {
        return false;
    }
    virtual bool visitBranch(TVisit, TIntermBranch* node) {
        return false;
    }
    virtual bool visitSwitch(TVisit, TIntermSwitch* node) {
        return false;
    }

    std::string sourceFile;
    TString structName;
    HeaderTraverseMode mode;
    TInfoSink out;
};

class HeaderWriterShader : public TShader {
public:
    HeaderWriterShader(EShLanguage language, std::string& unitFileName)
        : TShader(language), unitFileName(unitFileName) {}
    virtual ~HeaderWriterShader() {}

    bool writeHeaders() {
        FILE* headerf = fopen(binaryFileName, "a");
        if (!headerf) {
            fprintf(stderr, "fopen(%s): %d %s\n", binaryFileName, errno, strerror(errno));
            return true;
        }
        bool r = walkAST(headerf);
        fclose(headerf);
        return r;
    }

protected:
    std::string& unitFileName;
    bool walkAST(FILE* headerf) {
        if (intermediate->getTreeRoot() == 0) {
            fprintf(stderr, "%s: nullptr at treeRoot\n", binaryFileName);
            return true;
        }
        auto* p = variableName;
        if (!strncmp(p, "spv_", 4)) {
            p += strlen("spv_");
        }

        {
            HeaderOutputTraverser it(unitFileName, ModeCpp);
            it.structName = p;
            intermediate->getTreeRoot()->traverse(&it);
            fprintf(headerf, "/* Copyright (c) 2017-2018 the Volcano Authors. "
                    "Licensed under the GPLv3.\n"
                    " * THIS FILE IS AUTO-GENERATED. ANY EDITS WILL BE "
                    "DISCARDED.\n * Source file: %s\n"
                    " * See glslangValidator.{gni,py} which run "
                    "src/tools:copyHeader.\n */\ntypedef struct st_%s {\n%s",
                    unitFileName.c_str(), p, it.out.debug.c_str());
        }
        {
            HeaderOutputTraverser it(unitFileName, ModeAttributes);
            it.structName = p;
            intermediate->getTreeRoot()->traverse(&it);
            if (it.out.debug.c_str()[0] != 0) {
                fprintf(headerf, "\n#ifdef __cplusplus"
                        "\n    static std::vector<"
                        "VkVertexInputAttributeDescription"
                        "> getAttributes() {\n"
                        "        std::vector<VkVertexInputAttributeDescription"
                        "> attributes;\n"
                        "        VkVertexInputAttributeDescription* attr;\n"
                        "%s        return attributes;\n"
                        "    }\n"
                        "#endif /* __cplusplus */\n"
                        "} st_%s;\n", it.out.debug.c_str(), p);
            }
        }
        {
            HeaderOutputTraverser it(unitFileName, ModeUniforms);
            it.structName = p;
            intermediate->getTreeRoot()->traverse(&it);
            fprintf(headerf, "%s", it.out.debug.c_str());
        }
        {
            HeaderOutputTraverser it(unitFileName, ModeSpecialization);
            it.structName = p;
            intermediate->getTreeRoot()->traverse(&it);
            if (it.out.debug.c_str()[0] != 0) {
                fprintf(headerf, "#ifdef __cplusplus\n"
                        "struct " SPECIALIZATION_CONSTANTS_STRUCT_NAME
                        " {\n%s", it.out.debug.c_str());

                HeaderOutputTraverser im(unitFileName, ModeSpecializationMap);
                im.structName = p;
                intermediate->getTreeRoot()->traverse(&im);
                std::string stages = "VK_SHADER_STAGE_";
                switch (getStage()) {
                  case EShLangVertex: stages += "VERTEX_BIT"; break;
                  case EShLangTessControl: stages += "TESSELLATION_CONTROL_BIT"; break;
                  case EShLangTessEvaluation: stages += "TESSELLATION_EVALUATION_BIT"; break;
                  case EShLangGeometry: stages += "GEOMETRY_BIT"; break;
                  case EShLangFragment: stages += "FRAGMENT_BIT"; break;
                  case EShLangCompute: stages += "COMPUTE_BIT"; break;
                  case EShLangRayGenNV: stages += "RAYGEN_BIT_NV"; break;
                  case EShLangIntersectNV: stages += "INTERSECTION_BIT_NV"; break;
                  case EShLangAnyHitNV: stages += "ANY_HIT_BIT_NV"; break;
                  case EShLangClosestHitNV: stages += "CLOSEST_HIT_BIT_NV"; break;
                  case EShLangMissNV: stages += "MISS_BIT_NV"; break;
                  case EShLangCallableNV: stages += "CALLABLE_BIT_NV"; break;
                  case EShLangTaskNV: stages += "TASK_BIT_NV"; break;
                  case EShLangMeshNV: stages += "MESH_BIT_NV"; break;
                  case EShLangCount:
                      fprintf(stderr, "%s: invalid stage EshLanguage == EshLangCount\n",
                              binaryFileName);
                      return true;
                }
                const char argIndent[] = "                                  ";
                fprintf(headerf, "    WARN_UNUSED_RESULT int getMap("
                        "std::vector<VkSpecializationMapEntry>& map,\n"
                        "%sVkSpecializationInfo& info,\n"
                        "%sVkShaderStageFlags& stages) {\n"
                        "        map.clear();\n"
                        "        VkSpecializationMapEntry* entry;\n"
                        "%s\n        info.mapEntryCount = static_cast<"
                        "uint32_t>(map.size());\n"
                        "        info.pMapEntries = map.data();\n"
                        "        info.dataSize = sizeof(*this);\n"
                        "        info.pData = static_cast<void*>(this);\n"
                        "        stages = %s;\n"
                        "        return 0;\n"
                        "    }\n};\n#endif /* __cplusplus */\n", argIndent,
                        argIndent, im.out.debug.c_str(), stages.c_str());
            }
        }
        return false;  // false indicates success.
    }
};

bool CopyHeaderToOutput(std::list<TShader*>& shaders,
                        TProgram& program,
                        EShLanguage stage,
                        std::string unitFileName,
                        char** text,
                        const char* const* fileNameList,
                        bool outputPreprocessed,
                        bool flattenUniformArrays,
                        bool noStorageFormat,
                        bool autoMapBindings,
                        const int defaultVersion,
                        TBuiltInResource &Resources,
                        EShMessages &messages) {
    HeaderWriterShader* shader = new HeaderWriterShader(stage, unitFileName);
    shader->setStringsWithLengthsAndNames(text, NULL, fileNameList, 1);
    if (entryPointName) // HLSL todo: this needs to be tracked per compUnits
        shader->setEntryPoint(entryPointName);
    if (sourceEntryPointName)
        shader->setSourceEntryPoint(sourceEntryPointName);

    shader->setShiftSamplerBinding(baseSamplerBinding[stage]);
    shader->setShiftTextureBinding(baseTextureBinding[stage]);
    shader->setShiftImageBinding(baseImageBinding[stage]);
    shader->setShiftUboBinding(baseUboBinding[stage]);
    shader->setShiftSsboBinding(baseSsboBinding[stage]);
    shader->setFlattenUniformArrays(flattenUniformArrays);
    shader->setNoStorageFormat(noStorageFormat);

    if (autoMapBindings)
        shader->setAutoMapBindings(true);

    shaders.push_back(shader);

    if (outputPreprocessed) {
        std::string str;
        TShader::ForbidIncluder includer;
        bool failure = false;
        if (shader->preprocess(&Resources, defaultVersion, ENoProfile, false, false,
                               messages, &str, includer)) {
            if (str != "") puts(str.c_str());
        } else {
            failure = true;
        }
        return failure;
    }
    if (! shader->parse(&Resources, defaultVersion, false, messages))
        return true;

    program.addShader(shader);
    // Write to the header file by reconstructing parts of the AST.
    if (shader->writeHeaders()) {
        return true;
    }

    return false;
}

class QualifierToCpp : public TQualifier {
public:
    QualifierToCpp() = delete;
    QualifierToCpp(const TQualifier& copy) : TQualifier(copy) {}
    TString toString();
};

static void outIfToTString(TString* result, bool test, const char* fmt, ...)
        VOLCANO_PRINTF(3, 4);
static void outIfToTString(TString* result, bool test, const char* fmt, ...) {
    if (!test) {
        return;
    }
    int size = 0;
    va_list ap;
    va_start(ap, fmt);
    size = vsnprintf(NULL, size, fmt, ap);
    va_end(ap);
    if (size < 0) {
        result->append("vsnprintf(size) failed for \"");
        result->append(fmt);
        result->append("\"");
        return;
    }
    size++;
    char* buf = new char[size];
    va_start(ap, fmt);
    if (vsnprintf(buf, size, fmt, ap) < 0) {
        result->append("vsnprintf failed for \"");
        result->append(fmt);
        result->append("\"");
        delete[] buf;
        return;
    }
    va_end(ap);
    result->append(buf);
    delete[] buf;
};
#define outIf(...) outIfToTString(&result, __VA_ARGS__)

TString QualifierToCpp::toString() {
    TString result;

    if (hasLayout()) {
        // To reduce noise, skip "layout(" if the only layout is an
        // xfb_buffer with no triggering xfb_offset.
        TQualifier noXfbBuffer = *this;
        noXfbBuffer.layoutXfbBuffer = layoutXfbBufferEnd;
        if (noXfbBuffer.hasLayout()) {
            result.append(" layout(");
            outIf(hasAnyLocation(), " location=%u", layoutLocation);
            outIf(hasComponent(), " component=%u", layoutComponent);
            outIf(hasIndex(), " index=%u", layoutIndex);
            outIf(hasSet(), " set=%u", layoutSet);
            outIf(hasBinding(), " binding=%u", layoutBinding);
            outIf(hasStream(), " stream=%u", layoutStream);
            outIf(hasMatrix(), " %s", getLayoutMatrixString(layoutMatrix));
            outIf(hasPacking(), " %s", getLayoutPackingString(layoutPacking));
            outIf(hasOffset(), " offset=%d", layoutOffset);
            outIf(hasAlign(), " align=%d", layoutAlign);
            outIf(hasFormat(), " %s", getLayoutFormatString(layoutFormat));
            outIf(hasXfbBuffer() && hasXfbOffset(), " xfb_buffer=%u", layoutXfbBuffer);
            outIf(hasXfbOffset(), " xfb_offset=%u", layoutXfbOffset);
            outIf(hasXfbStride(), " xfb_stride=%u", layoutXfbStride);
            outIf(hasAttachment(), " input_attachment_index=%u", layoutAttachment);
            outIf(hasSpecConstantId(), " constant_id=%u", layoutSpecConstantId);
            outIf(layoutPushConstant, " push_constant");

#ifdef NV_EXTENSIONS
            outIf(layoutPassthrough, " passthrough");
            outIf(layoutViewportRelative, " layoutViewportRelative");
            outIf(layoutSecondaryViewportRelativeOffset != -2048, " layoutSecondaryViewportRelativeOffset=%d", layoutSecondaryViewportRelativeOffset);
#endif
            result.append(")");
        }
    }

    outIf(invariant, " invariant");
    outIf(noContraction, " noContraction");
    outIf(centroid, " centroid");
    outIf(smooth, " smooth");
    outIf(flat, " flat");
    outIf(nopersp, " noperspective");
#ifdef AMD_EXTENSIONS
    outIf(explicitInterp, " __explicitInterpAMD");
#endif
    outIf(patch, " patch");
    outIf(sample, " sample");
    outIf(coherent, " coherent");
    outIf(volatil, " volatile");
    outIf(restrict, " restrict");
    outIf(readonly, " readonly");
    outIf(writeonly, " writeonly");
    outIf(specConstant, " specialization");
    outIf(precision != EpqNone, " %s", GetPrecisionQualifierString(precision));
    outIf(true, " %s", GetStorageQualifierString(storage));

    if (result.find(" ") == 0) {
        result.erase(0, 1);
    }
    return result;
}

TString TypeToCpp::toVertexInputAttributes() {
    TString spaces;
    for (int i = 0; i < indent; i++) spaces.append("    ");
    TString result;
    result.append("\n");
    result.append(spaces);
    result.append(autoPrefix);

    QualifierToCpp qualifier(type.getQualifier());

    // Whitelist the storage qualifiers that will be included.
    switch (qualifier.storage) {
    case EvqVaryingIn:
    case EvqIn:
    case EvqInOut:
      break;

    default:
      // All other storage qualifiers are filtered out.
      return "";
    }
    // Filter out all symbols that are predefined.
    if (qualifier.builtIn != EbvNone || type.getBasicType() == EbtVoid) {
        return "";
    }
    result.append("/*");
    result.append(qualifier.toString());
    result.append("*/\n");
    result.append(spaces);

    // If fieldName begins with "anon@", attempt to unpack without creating
    // a struct.
    bool fieldIsAnon = IsAnonymous(fieldName);

    // Add struct/block members, but only if fieldIsAnon.
    if (fieldIsAnon) {
        if (type.isStruct()) {
            auto* structure = type.getStruct();

            TString childAutoPrefix;
            childAutoPrefix = result;
            result.clear();

            while (childAutoPrefix[0] == ' ') childAutoPrefix.erase(0, 1);

            for (size_t i = 0; i < structure->size(); ++i) {
                int childIndent = indent;
                TypeToCpp converter(*(*structure)[i].type,
                                    (*structure)[i].type->getFieldName(), structName,
                                    childAutoPrefix, childIndent, mode);
                result.append(converter.toVertexInputAttributes());
            }
        }
        return result;
    }
    // From here on, assume fieldIsAnon == false
    auto basicType = type.getBasicType();
    switch (basicType) {
    case EbtSampler:
        if (type.getSampler().isImage() || type.getSampler().isCombined()) {
            result.append("extern VkDescriptorImageInfo /*for descriptor write*/");
        } else if (type.getSampler().isSubpass()) {
            result.append("extern VkAttachmentReference /*for VkSubpassDescription::pInputAttachments*/");
        } else if (type.getSampler().isPureSampler()) {
            result.append("//{GLSL 'sampler'}");
        } else {
            // Should be 'texture', 'i8texture' or 'u16texture', etc. unless new types are added.
            result.append("//{GLSL '");
            result.append(type.getSampler().getString());
            result.append("'}");
        }
        break;
    default:
        break;
    }

    unsigned bindingNumber = qualifier.hasBinding() ? qualifier.layoutBinding : 0;
    TString attrBinding = std::to_string(bindingNumber).c_str();
    unsigned locationNumber = qualifier.hasAnyLocation() ? qualifier.layoutLocation : 0;
    TString attrLocation = std::to_string(locationNumber).c_str();

    TString attrFormat;
    switch (type.getBasicType()) {
    case EbtSampler:
        result.append("<Sampler should be possible. Not implemented yet, sorry!>");
        return result;
    case EbtVoid:
        attrFormat = "void /*should not be possible, this is filtered out*/";
        break;
    case EbtStruct:
        // ignore Struct types, unless it is handled by fieldIsAnon.
        return result;
    case EbtBlock:
        // ignore Block types, unless it is handled by fieldIsAnon.
        return result;
    case EbtString:
        result.append("<HLSL string is invalid in this context>");
        return result;
    case EbtNumTypes:
        result.append("<NumTypes is invalid in this context>");
        return result;
    case EbtFloat16:
    case EbtFloat:       makeFormat(attrFormat, 32, "SFLOAT"); break;
    case EbtDouble:      makeFormat(attrFormat, 64, "SFLOAT"); break;
    case EbtInt:         makeFormat(attrFormat, 32, "SINT"); break;
    case EbtUint:        makeFormat(attrFormat, 32, "UINT"); break;
    case EbtInt64:       makeFormat(attrFormat, 64, "SINT"); break;
    case EbtUint64:      makeFormat(attrFormat, 64, "UINT"); break;
    case EbtBool:
    case EbtUint8:       makeFormat(attrFormat,  8, "UINT"); break;
    case EbtInt8:        makeFormat(attrFormat,  8, "SINT"); break;
    case EbtInt16:       makeFormat(attrFormat, 16, "SINT"); break;
    case EbtUint16:      makeFormat(attrFormat, 16, "UINT"); break;
    case EbtAtomicUint:  makeFormat(attrFormat, 32, "UINT"); break;
    }

    TString attrCode = "attributes.emplace_back();\n" +
        spaces + "attr = &attributes.back();\n" +
        spaces + "attr->binding = " + attrBinding + ";\n" +
        spaces + "attr->location = " + attrLocation + ";\n" +
        spaces + "attr->format = " + attrFormat + ";\n" +
        spaces + "attr->offset = offsetof(st_" + structName + ", " +
            fieldName + ");\n";
    result.append(attrCode);
    return result;
}

TString TypeToCpp::toSpecializationMap() {
    TString spaces;
    for (int i = 0; i < indent; i++) spaces.append("    ");
    TString result;
    result.append("\n");
    result.append(spaces);
    result.append(autoPrefix);

    QualifierToCpp qualifier(type.getQualifier());

    // Whitelist the storage qualifiers that will be included.
    if (!qualifier.hasSpecConstantId()) {
        return "";
    }
    switch (qualifier.storage) {
        case EvqConst:
        case EvqConstReadOnly:
      break;

    default:
      // All other storage qualifiers are filtered out.
      return "";
    }
    result.append("/*");
    result.append(qualifier.toString());
    result.append("*/\n");
    result.append(spaces);

    // If fieldName begins with "anon@", attempt to unpack without creating
    // a struct.
    bool fieldIsAnon = IsAnonymous(fieldName);

    // Add struct/block members, but only if fieldIsAnon.
    if (fieldIsAnon) {
        if (type.isStruct()) {
            // Note: glslangValidator at the moment silently ignores
            // "layout(constant_id = N) struct FOO { int bar }", but
            // if you provide a struct initializer it complains,
            // "only scalar type allowed in constant." This code for handling
            // specialization nested structs may be useful in the future.
            auto* structure = type.getStruct();

            TString childAutoPrefix;
            childAutoPrefix = result;
            result.clear();

            while (childAutoPrefix[0] == ' ') childAutoPrefix.erase(0, 1);

            for (size_t i = 0; i < structure->size(); ++i) {
                int childIndent = indent;
                TypeToCpp converter(*(*structure)[i].type,
                                    (*structure)[i].type->getFieldName(), structName,
                                    childAutoPrefix, childIndent, mode);
                result.append(converter.toVertexInputAttributes());
            }
        }
        return result;
    }
    // From here on, assume fieldIsAnon == false
    auto basicType = type.getBasicType();
    switch (basicType) {
    case EbtSampler:
        if (type.getSampler().isImage() || type.getSampler().isCombined()) {
            result.append("extern VkDescriptorImageInfo /*for descriptor write*/");
        } else if (type.getSampler().isSubpass()) {
            result.append("extern VkAttachmentReference /*for VkSubpassDescription::pInputAttachments*/");
        } else if (type.getSampler().isPureSampler()) {
            result.append("//{GLSL 'sampler'}");
        } else {
            // Should be 'texture', 'i8texture' or 'u16texture', etc. unless new types are added.
            result.append("//{GLSL '");
            result.append(type.getSampler().getString());
            result.append("'}");
        }
        break;
    default:
        break;
    }

    TString attrId = std::to_string(qualifier.layoutSpecConstantId).c_str();
    TString attrCode = "map.emplace_back();\n" +
        spaces + "entry = &map.back();\n" +
        spaces + "entry->constantID = " + attrId + ";\n" +
        spaces + "entry->offset = offsetof("
            SPECIALIZATION_CONSTANTS_STRUCT_NAME ", " +
            fieldName + ");\n" +
        spaces + "entry->size = sizeof("
            SPECIALIZATION_CONSTANTS_STRUCT_NAME "::" +
            fieldName + ");\n";
    result.append(attrCode);
    return result;
}

bool TypeToCpp::typeNeedsCustomGLM() {
    bool canUseGLM = false;
    if (type.isMatrix()) {
        if (type.getMatrixCols() < 2 || type.getMatrixCols() > 4) {
            return false;
        }
        if (type.getMatrixRows() < 2 || type.getMatrixRows() > 4) {
            return false;
        }
        canUseGLM = !type.isVector();
    }
    if (type.isVector()) {
        if (type.getVectorSize() < 1 || type.getVectorSize() > 4) {
            return false;
        }
        canUseGLM = !type.isMatrix();
    }
    if (!canUseGLM) {
        return false;
    }

    switch (type.getBasicType()) {
    case EbtSampler: return false;
    case EbtVoid: return false;
    case EbtFloat: return true;
    case EbtDouble: return true;
    case EbtFloat16: return true;
    case EbtInt8: return type.isVector();
    case EbtUint8: return type.isVector();
    case EbtInt16: return type.isVector();
    case EbtUint16: return type.isVector();
    case EbtInt: return type.isVector();
    case EbtUint: return type.isVector();
    case EbtInt64: return type.isVector();
    case EbtUint64: return type.isVector();
    case EbtBool: return false;
    case EbtAtomicUint: return false;
    case EbtStruct: return false;
    case EbtBlock: return false;
    case EbtString:
        fprintf(stderr, "typeNeedsCustomGLM: HLSL string is invalid in this context\n");
        return false;
    case EbtNumTypes:
        fprintf(stderr, "typeNeedsCustomGLM: NumTypes is invalid in this context\n");
        return false;
    }
    fprintf(stderr, "typeNeedsCustomGLM: unsupported type %d\n",
            (int) type.getBasicType());
    return false;
}

TString TypeToCpp::toCpp()
{
    if (mode == ModeAttributes) {
        return toVertexInputAttributes();
    } else if (mode == ModeSpecializationMap) {
        return toSpecializationMap();
    }

    TString result;
    bool isConst = false;
    // Only "const"/"uniform"/"buffer"/"shared" when mode == ModeUniforms.
    // "global" is always shown (it is typically inside another block).
    switch (type.getQualifier().storage) {
    case EvqGlobal: break;
    case EvqConst:
    case EvqConstReadOnly:
        isConst = true; /* Falls through. */
    case EvqUniform:
    case EvqBuffer:
    case EvqShared:
        if (mode != ModeUniforms && mode != ModeSpecialization) {
            return result;
        }
        break;
    default:
        // In/out/builtin when mode == ModeCpp or mode == ModeAttributes.
        if (mode != ModeCpp && mode != ModeAttributes) {
            return result;
        }
    }
    for (int i = 0; i < indent; i++) result.append("    ");
    result.append(autoPrefix);

    QualifierToCpp qualifier(type.getQualifier());

    bool predefined;
    if (isConst) {
        // The const symbols are predefined but need a special case.
        predefined = true;
        if (qualifier.isFrontEndConstant()) {
            result.append("//const ");
            // TODO: if TIntermSymbol* node got passed in, see how
            // TParseContext::handleBracketDereference() calls getIConst() to
            // extract the defined value of the constant.
        } else if (qualifier.hasSpecConstantId()) {
            // Specialization constants are output in ModeSpecialization only.
            if (mode != ModeSpecialization) {
                return TString();
            }
        }
    } else {
        if (mode == ModeSpecialization && type.getQualifier().storage == EvqUniform) {
            // Skip uniform storage in ModeSpecialization
            return TString();
        }
        // Filter out all symbols that are predefined.
        predefined = false;
        if (qualifier.storage == EvqVaryingOut || qualifier.storage == EvqOut ||
            qualifier.builtIn != EbvNone || type.getBasicType() == EbtVoid)
        {
            predefined = true;
            if (autoPrefix.find("//") != 0) {
                result.append("//");
            }
        } else {
            result.append("/*");
        }
        result.append(qualifier.toString());
        if (!predefined) {
            result.append("*/\n");
            for (int i = 0; i < indent; i++) result.append("    ");
        } else {
            result.append(" ");
        }
    }

    // If fieldName begins with "anon@", attempt to unpack without creating
    // a struct.
    bool fieldIsAnon = IsAnonymous(fieldName);
    bool addGLM = !fieldIsAnon && autoPrefix.empty() && !predefined && typeNeedsCustomGLM();
    if (addGLM) {
        outIf(true, "#ifndef GLM_VERSION_MAJOR /* glm not available */\n");
        for (int i = 0; i < indent; i++) result.append("    ");
    }
    if (!fieldIsAnon) {
        switch (type.getBasicType()) {
        case EbtSampler:
            if (type.getSampler().isImage() || type.getSampler().isCombined()) {
                result.append("extern VkDescriptorImageInfo /*for descriptor write*/");
            } else if (type.getSampler().isSubpass()) {
                result.append("extern VkAttachmentReference /*for VkSubpassDescription::pInputAttachments*/");
            } else if (type.getSampler().isPureSampler()) {
                result.append("//{GLSL 'sampler'}");
            } else {
                // Should be 'texture', 'i8texture' or 'u16texture', etc. unless new types are added.
                result.append("//{GLSL '");
                result.append(type.getSampler().getString());
                result.append("'}");
            }
            break;
        case EbtVoid:        result.append("void"); break;
        case EbtFloat:       result.append("float"); break;
        case EbtDouble:      result.append("double"); break;
        case EbtFloat16:     result.append("float16_t"); break;
        case EbtInt8:        result.append("int8_t"); break;
        case EbtUint8:       result.append("uint8_t"); break;
        case EbtInt16:       result.append("int16_t"); break;
        case EbtUint16:      result.append("uint_t"); break;
        case EbtInt:         result.append("int"); break;
        case EbtUint:        result.append("uint"); break;
        case EbtInt64:       result.append("int64_t"); break;
        case EbtUint64:      result.append("uint64_t"); break;
        case EbtBool:        result.append("bool"); break;
        case EbtAtomicUint:  result.append("atomic_uint"); break;
        case EbtStruct:      result.append("struct"); break;
        case EbtBlock:       result.append("struct"); break;
        case EbtString:
            result.append("<HLSL string is invalid in this context>");
            break;
        case EbtNumTypes:
            result.append("<NumTypes is invalid in this context>");
            break;
        }
    }

    // Add struct/block members
    if (type.isStruct()) {
        auto* structure = type.getStruct();

        TString childAutoPrefix;
        if (fieldIsAnon) {
            childAutoPrefix = result;
            result.clear();

            while (childAutoPrefix[0] == ' ') childAutoPrefix.erase(0, 1);
        }
        outIf(!fieldIsAnon, " %s {\n", type.getTypeName().c_str());
        for (size_t i = 0; i < structure->size(); ++i) {
            int childIndent = indent;
            if (!fieldIsAnon) childIndent++;
            TypeToCpp converter(*(*structure)[i].type,
                                (*structure)[i].type->getFieldName(), structName,
                                childAutoPrefix, childIndent, mode);
            result.append(converter.toCpp());
        }
        if (!fieldIsAnon) {
            for (int i = 0; i < indent; i++) result.append("    ");
            result.append("}");
        }
    }

    if (!fieldIsAnon) {
        //outIf(qualifier.builtIn != EbvNone, " /*builtIn:%s*/", type.getBuiltInVariableString());
        if (mode == ModeUniforms && type.isStruct() && indent == 0) {
            // Suppress fieldName and any isMatrix()/isVector().
        } else {
            outIf(true, " %s", fieldName.c_str());

            if (type.isArray()) {
                auto* arraySizes = type.getArraySizes();
                for(int i = 0; i < (int)arraySizes->getNumDims(); ++i) {
                    if (i == 0) result.append(" ");
                    int size = arraySizes->getDimSize(i);
                    result.append("[");
                    outIf(size != 0, "%d", size);
                    result.append("]");
                }
            }
            outIf(type.isMatrix(), " [%d][%d]", type.getMatrixCols(), type.getMatrixRows());
            outIf(type.isVector(), " [%d]", type.getVectorSize());
        }
        outIf(true, ";\n");

        if (addGLM) {
            for (int i = 0; i < indent; i++) result.append("    ");
            outIf(true, "#else /* ifdef GLM_VERSION_MAJOR then use glm */\n");
            for (int i = 0; i < indent; i++) result.append("    ");

            if (type.isArray()) {
                auto* arraySizes = type.getArraySizes();
                for(int i = 0; i < (int)arraySizes->getNumDims(); ++i) {
                    if (i == 0) result.append(" ");
                    int size = arraySizes->getDimSize(i);
                    result.append("[");
                    outIf(size != 0, "%d", size);
                    result.append("]");
                }
            }
            result.append("glm::");
            switch (type.getBasicType()) {
            case EbtFloat: break;
            case EbtDouble: result.append("d"); break;
            case EbtFloat16: result.append("f16"); break;
            // Note: the following only work as vector, checked already above.
            case EbtInt8: result.append("i8"); break;
            case EbtUint8: result.append("u8"); break;
            case EbtInt16: result.append("i16"); break;
            case EbtUint16: result.append("u16"); break;
            case EbtInt: result.append("i32"); break;
            case EbtUint: result.append("u32"); break;
            case EbtInt64: result.append("i64"); break;
            case EbtUint64: result.append("u64"); break;
            default:
                result.append("<Type in typeNeedsCustomGLM() missing in toCpp()>");
                break;
            }
            outIf(type.isMatrix(), "mat%dx%d", type.getMatrixCols(), type.getMatrixRows());
            outIf(type.isVector(), "vec%d", type.getVectorSize());
            outIf(true, " %s;\n", fieldName.c_str());
            for (int i = 0; i < indent; i++) result.append("    ");
            outIf(true, "#endif /* GLM_VERSION_MAJOR */\n");
        }
        if (mode == ModeUniforms) {
            if (type.isMatrix() && type.getMatrixCols() == 3) {
                result.append("#warning In UBO/SSBO/PushConstant, ");
                result.append("Matrix of 3 cols is broken, please use 4: ");
                result.append("https://stackoverflow.com/a/38172697/734069\n");
            } else if (type.isVector() && type.getVectorSize() == 3) {
                result.append("#warning In UBO/SSBO/PushConstant, ");
                result.append("vec3 is broken, please use vec4: ");
                result.append("https://stackoverflow.com/a/38172697/734069\n");
            }
            if (qualifier.hasBinding()) {
                const char* typeName;
                if (type.isStruct()) {
                    typeName = type.getTypeName().c_str();
                } else {
                    typeName = fieldName.c_str();
                }
                for (int i = 0; i < indent; i++) result.append("    ");
                outIf(true, "WARN_UNUSED_RESULT inline unsigned bindingIndexOf%s", typeName);
                outIf(true,"() { return %u; };\n", qualifier.layoutBinding);
            }
        }
    }
    return result;
}

// makeFormat returns a string that evaluates to a value from VkFormat.
void TypeToCpp::makeFormat(TString& format, int bits, const char* suffix) {
    int elements = 1;
    if (type.isArray()) {
        auto* arraySizes = type.getArraySizes();
        for(int i = 0; i < (int)arraySizes->getNumDims(); ++i) {
            elements *= arraySizes->getDimSize(i);
        }
    }
    if (type.isMatrix()) {
        elements *= type.getMatrixCols();
        elements *= type.getMatrixRows();
    }
    if (type.isVector()) {
        elements *= type.getVectorSize();
    }

    static const char* fields[] = { "R", "G", "B", "A" };
    format = "VK_FORMAT_";
    if (elements > static_cast<decltype(elements)>(sizeof(fields)/sizeof(fields[0]))) {
        fprintf(stderr, "makeFormat: elements=%d cannot be encoded in RGBA.\n",
                elements);
        exit(1);
    }
    for (int field = 0; field < elements; field++) {
        format.append(fields[field]);
        format.append(std::to_string(bits).c_str());
    }
    format.append("_");
    format.append(suffix);
}
