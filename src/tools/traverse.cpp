/* Copyright (c) 2017 the Volcano Authors. Licensed under the GPLv3.
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

class TypeToCpp {
public:
    TypeToCpp(const TType& type, const TString& fieldName, TString autoPrefix,
              int indent, bool showUniforms)
        : indent(indent)
        , type(type)
        , fieldName(fieldName)
        , autoPrefix(autoPrefix)
        , showUniforms(showUniforms) {}
    ~TypeToCpp() {}

    TString toCpp();
    TString toVertexInputAttributes(TString& structName);

protected:
    void makeFormat(TString& format, int bits, const char* suffix);

    const int indent;
    const TType& type;
    TString fieldName;
    TString autoPrefix;
    bool showUniforms;
};

enum HeaderTraverseMode {
    ModeCpp,  // Output fields a c++, skipping uniforms.
    ModeAttributes,  // Output VkVertexInputAttributeDescription.
    ModeUniforms,  // Output uniform fields as c++.
};

class HeaderOutputTraverser : public TIntermTraverser {
public:
    HeaderOutputTraverser(std::string& unitFileName, HeaderTraverseMode mode)
        : sourceFile(unitFileName), foundLinkerObjects(false), mode(mode) {}

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
            switch (mode) {
            case ModeCpp:
                out.debug << "/* Copyright (c) 2017 the Volcano Authors. "
                    "Licensed under the GPLv3.\n"
                    " * THIS FILE IS AUTO-GENERATED. ANY EDITS WILL BE DISCARDED.\n"
                    " * Source file: " << sourceFile << "\n"
                    " * See glslangValidator.{gni,py} which run "
                    "src/tools:copyHeader.\n */\n"
                    "typedef struct st_" << structName << " {\n";
                break;
            case ModeAttributes:
                out.debug << "\n#ifdef __cplusplus"
                    "\n    static std::vector<"
                                    "VkVertexInputAttributeDescription"
                                    "> getAttributes() {\n"
                    "        std::vector<VkVertexInputAttributeDescription> "
                                    "attributes;\n"
                    "        VkVertexInputAttributeDescription* attr;\n";
                break;
            case ModeUniforms:
                break;
            }
            foundLinkerObjects = true;
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
        switch (mode) {
        case ModeCpp:
        {
            TypeToCpp converter(t->getType(), node->getName(), "", 1, false);
            out.debug << converter.toCpp();
            break;
        }
        case ModeAttributes:
        {
            TypeToCpp converter(t->getType(), node->getName(), "", 2, false);
            out.debug << converter.toVertexInputAttributes(structName);
            break;
        }
        case ModeUniforms:
        {
            TypeToCpp converter(t->getType(), node->getName(), "", 0, true);
            out.debug << converter.toCpp();
            break;
        }
        }
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
    bool foundLinkerObjects;
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
            if (!it.foundLinkerObjects) {
                fprintf(stderr, "%s: linker objects not found\n", binaryFileName);
                return true;
            }
            fprintf(headerf, "%s", it.out.debug.c_str());
        }
        {
            HeaderOutputTraverser it(unitFileName, ModeAttributes);
            it.structName = p;
            intermediate->getTreeRoot()->traverse(&it);
            if (it.foundLinkerObjects) {
                it.out.debug << "        return attributes;\n"
                                "    }\n"
                                "#endif /* __cplusplus */\n"
                                "} st_" << p << ";\n";
            }
            fprintf(headerf, "%s", it.out.debug.c_str());
        }
        {
            HeaderOutputTraverser it(unitFileName, ModeUniforms);
            it.structName = p;
            intermediate->getTreeRoot()->traverse(&it);
            fprintf(headerf, "%s", it.out.debug.c_str());
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
    outIf(specConstant, " specialization-constant");
    outIf(precision != EpqNone, " %s", GetPrecisionQualifierString(precision));
    outIf(true, " %s", GetStorageQualifierString(storage));

    if (result.find(" ") == 0) {
        result.erase(0, 1);
    }
    return result;
}

TString TypeToCpp::toCpp()
{
    TString result;
    bool isConst = false;
    // Only "const"/"uniform"/"buffer"/"shared" when showUniforms == true.
    // Only in/out/builtin type data when showUniforms == false.
    // "global" is always shown (it is typically inside another block).
    switch (type.getQualifier().storage) {
    case EvqGlobal: break;
    case EvqConst:
    case EvqConstReadOnly:
        isConst = true; /* Falls through. */
    case EvqUniform:
    case EvqBuffer:
    case EvqShared:
        if (!showUniforms) {
            return result;
        }
        break;
    default:
        if (showUniforms) {
            return result;
        }
    }
    for (int i = 0; i < indent; i++) result.append("    ");
    result.append(autoPrefix);

    QualifierToCpp qualifier(type.getQualifier());

    if (isConst) {
        // The const symbols are predefined but need a special case.
        if (qualifier.isFrontEndConstant()) {
            result.append("//const ");
            // TODO: if TIntermSymbol* node got passed in, see how
            // TParseContext::handleBracketDereference() calls getIConst() to
            // extract the defined value of the constant.
        } else {
            outIf(true, "//[constant_id = %d] specialization-const ",
                  qualifier.layoutSpecConstantId);
        }
    } else {
        // Filter out all symbols that are predefined.
        bool predefined = false;
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
    if (!fieldIsAnon) {
        switch (type.getBasicType()) {
        case EbtSampler:
            result.append(type.getSampler().getString());
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
                                (*structure)[i].type->getFieldName(),
                                childAutoPrefix, childIndent, showUniforms);
            result.append(converter.toCpp());
        }
        if (!fieldIsAnon) {
            for (int i = 0; i < indent; i++) result.append("    ");
            result.append("}");
        }
    }

    if (!fieldIsAnon) {
        //outIf(qualifier.builtIn != EbvNone, " /*builtIn:%s*/", type.getBuiltInVariableString());
        if (showUniforms && type.isStruct() && indent == 0) {
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
        if (showUniforms) {
            if (type.isMatrix() && type.getMatrixCols() == 3) {
                result.append("#warning In UBO/SSBO/PushConstant, ");
                result.append("Matrix of 3 cols is broken, please use 4: ");
                result.append("https://stackoverflow.com/a/38172697/734069\n");
            } else if (type.isVector() && type.getVectorSize() == 3) {
                result.append("#warning In UBO/SSBO/PushConstant, ");
                result.append("vec3 is broken, please use vec4: ");
                result.append("https://stackoverflow.com/a/38172697/734069\n");
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

TString TypeToCpp::toVertexInputAttributes(TString& structName) {
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
    bool predefined = false;
    if (qualifier.builtIn != EbvNone || type.getBasicType() == EbtVoid) {
        return "";
    }
    result.append("/*");
    result.append(qualifier.toString());
    if (!predefined) {
        result.append("*/\n");
        result.append(spaces);
    } else {
        result.append(" ");
    }

    // If fieldName begins with "anon@", attempt to unpack without creating
    // a struct.
    bool fieldIsAnon = fieldName.find("anon@") == 0;
    if (!fieldIsAnon) {
        auto basicType = type.getBasicType();
        switch (basicType) {
        case EbtSampler:
            result.append(type.getSampler().getString());
            break;
        default:
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
        outIf(!fieldIsAnon, " %s {\n", fieldName.c_str());
        for (size_t i = 0; i < structure->size(); ++i) {
            int childIndent = indent;
            if (fieldIsAnon) {
                TypeToCpp converter(*(*structure)[i].type,
                                    (*structure)[i].type->getFieldName(),
                                    childAutoPrefix, childIndent, showUniforms);
                result.append(converter.toVertexInputAttributes(structName));
            }
        }
        if (!fieldIsAnon) {
            result.append(spaces);
            result.append("}");
        }
    }

    if (!fieldIsAnon) {
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
        case EbtInt:         makeFormat(attrFormat, 32, "INT"); break;
        case EbtUint:        makeFormat(attrFormat, 32, "UINT"); break;
        case EbtInt64:       makeFormat(attrFormat, 64, "INT"); break;
        case EbtUint64:      makeFormat(attrFormat, 64, "UINT"); break;
        case EbtBool:
        case EbtUint8:       makeFormat(attrFormat, 8, "UINT"); break;
        case EbtInt8:        makeFormat(attrFormat, 8, "INT"); break;
        case EbtInt16:       makeFormat(attrFormat, 16, "INT"); break;
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
    }
    return result;
}
