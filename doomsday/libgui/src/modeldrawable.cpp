/** @file modeldrawable.cpp  Drawable specialized for 3D models.
 *
 * @authors Copyright (c) 2014 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "de/ModelDrawable"

#include <de/App>
#include <de/ByteArrayFile>
#include <de/Matrix>
#include <de/GLBuffer>
#include <de/GLProgram>
#include <de/GLState>
#include <de/GLUniform>

#include <assimp/IOStream.hpp>
#include <assimp/IOSystem.hpp>
#include <assimp/Importer.hpp>
#include <assimp/LogStream.hpp>
#include <assimp/DefaultLogger.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

namespace de {
namespace internal {

/// Adapter between de::File and Assimp.
class ImpIOStream : public Assimp::IOStream
{
public:
    ImpIOStream(ByteArrayFile const &file) : _file(file), _pos(0)
    {}

    size_t Read(void *pvBuffer, size_t pSize, size_t pCount)
    {
        size_t const num = pSize * pCount;
        _file.get(_pos, reinterpret_cast<IByteArray::Byte *>(pvBuffer), num);
        _pos += num;
        return pCount;
    }

    size_t Write(void const *, size_t, size_t)
    {
        throw Error("ImpIOStream::Write", "Writing not allowed");
    }

    aiReturn Seek(size_t pOffset, aiOrigin pOrigin)
    {
        switch(pOrigin)
        {
        case aiOrigin_SET:
            _pos = pOffset;
            break;

        case aiOrigin_CUR:
            _pos += pOffset;
            break;

        case aiOrigin_END:
            _pos = _file.size() - pOffset;
            break;

        default:
            break;
        }
        return aiReturn_SUCCESS;
    }

    size_t Tell() const
    {
        return _pos;
    }

    size_t FileSize() const
    {
        return _file.size();
    }

    void Flush() {}

private:
    ByteArrayFile const &_file;
    size_t _pos;
};

/// Adapter between libdeng2 FS and Assimp.
struct ImpIOSystem : public Assimp::IOSystem
{
    ImpIOSystem() {}
    ~ImpIOSystem() {}

    char getOsSeparator() const { return '/'; }

    bool Exists(char const *pFile) const
    {
        return App::rootFolder().has(pFile);
    }

    Assimp::IOStream *Open(char const *pFile, char const *)
    {
        String const path = pFile;
        DENG2_ASSERT(!path.contains("\\"));
        return new ImpIOStream(App::rootFolder().locate<ByteArrayFile const>(path));
    }

    void Close(Assimp::IOStream *pFile)
    {
        delete pFile;
    }
};

struct ImpLogger : public Assimp::LogStream
{
    void write(char const *message)
    {
        LOG_RES_VERBOSE("[ai] %s") << message;
    }

    static void registerLogger()
    {
        if(registered) return;

        registered = true;
        Assimp::DefaultLogger::get()->attachStream(
                    new ImpLogger,
                    Assimp::Logger::Info |
                    Assimp::Logger::Warn |
                    Assimp::Logger::Err);
    }

    static bool registered;
};

bool ImpLogger::registered = false;

} // namespace internal
using namespace internal;

#define MAX_BONES               64
#define MAX_BONES_PER_VERTEX    4

struct ModelVertex
{
    Vector3f pos;
    Vector3f normal;
    Vector3f tangent;
    Vector3f bitangent;
    Vector2f texCoord;
    Vector4f boneIds;
    Vector4f boneWeights;

    LIBGUI_DECLARE_VERTEX_FORMAT(7)
};

AttribSpec const ModelVertex::_spec[7] = {
    { AttribSpec::Position,    3, GL_FLOAT, false, sizeof(ModelVertex), 0 },
    { AttribSpec::Normal,      3, GL_FLOAT, false, sizeof(ModelVertex), 3 * sizeof(float) },
    { AttribSpec::Tangent,     3, GL_FLOAT, false, sizeof(ModelVertex), 6 * sizeof(float) },
    { AttribSpec::Bitangent,   3, GL_FLOAT, false, sizeof(ModelVertex), 9 * sizeof(float) },
    { AttribSpec::TexCoord0,   2, GL_FLOAT, false, sizeof(ModelVertex), 12 * sizeof(float) },
    { AttribSpec::BoneIDs,     4, GL_FLOAT, false, sizeof(ModelVertex), 14 * sizeof(float) },
    { AttribSpec::BoneWeights, 4, GL_FLOAT, false, sizeof(ModelVertex), 18 * sizeof(float) },
};
LIBGUI_VERTEX_FORMAT_SPEC(ModelVertex, 22 * sizeof(float))

static Matrix4f convertMatrix(aiMatrix4x4 const &aiMat)
{
    return Matrix4f(&aiMat.a1).transpose();
}

DENG2_PIMPL(ModelDrawable)
{
    typedef GLBufferT<ModelVertex> VBuf;

    struct VertexBone
    {
        duint16 ids[MAX_BONES_PER_VERTEX];
        dfloat weights[MAX_BONES_PER_VERTEX];

        VertexBone()
        {
            zap(ids);
            zap(weights);
        }
    };

    struct BoneData
    {
        Matrix4f offset;
    };

    Asset modelAsset;
    Path sourcePath;
    Assimp::Importer importer;
    aiScene const *scene;

    QVector<VertexBone> vertexBones; // indexed by vertex
    QHash<String, duint16> boneNameToIndex;
    QVector<BoneData> bones; // indexed by bone index
    Vector3f minPoint; ///< Bounds in default pose.
    Vector3f maxPoint;
    Matrix4f globalInverse;
    ddouble animTime;

    VBuf *buffer;
    GLProgram *program;
    GLUniform uBoneMatrices;

    Instance(Public *i)
        : Base(i)
        , scene(0)
        , animTime(0)
        , buffer(0)
        , program(0)
        , uBoneMatrices("uBoneMatrices", GLUniform::Mat4Array, MAX_BONES)
    {
        // Use libdeng2 for file access.
        importer.SetIOHandler(new ImpIOSystem);

        // Get most kinds of log output.
        ImpLogger::registerLogger();
    }

    ~Instance()
    {
        glDeinit();
    }

    void import(File const &file)
    {
        LOG_RES_MSG("Loading model from %s") << file.description();

        scene = 0;
        sourcePath = file.path();

        if(!importer.ReadFile(sourcePath.toString().toLatin1(),
                              aiProcess_CalcTangentSpace |
                              aiProcess_GenSmoothNormals |
                              aiProcess_JoinIdenticalVertices |
                              aiProcess_Triangulate |
                              aiProcess_GenUVCoords |
                              aiProcess_SortByPType ))
        {
            throw LoadError("ModelDrawable::import", String("Failed to load model from %s: %s")
                            .arg(file.description()).arg(importer.GetErrorString()));
        }

        scene = importer.GetScene();
    }

    /// Release all loaded model data.
    void clear()
    {
        glDeinit();

        sourcePath.clear();
        importer.FreeScene();
        scene = 0;
    }

    void glInit()
    {
        DENG2_ASSERT_IN_MAIN_THREAD();

        if(modelAsset.isReady())
        {
            // Already good to go.
            return;
        }

        // Has a scene been imported successfully?
        if(!scene) return;

        initFromScene();

        // Ready to go!
        modelAsset.setState(Ready);
    }

    void glDeinit()
    {
        /// @todo Free all GL resources.
        delete buffer;
        buffer = 0;

        vertexBones.clear();
        bones.clear();
        boneNameToIndex.clear();

        modelAsset.setState(NotReady);
    }

    void initFromScene()
    {
        globalInverse = convertMatrix(scene->mRootNode->mTransformation).inverse();
        maxPoint      = Vector3f(1.0e-9, 1.0e-9, 1.0e-9);
        minPoint      = Vector3f(1.0e9,  1.0e9,  1.0e9);

        // Determine the total bounding box.
        for(duint i = 0; i < scene->mNumMeshes; ++i)
        {
            aiMesh const &mesh = *scene->mMeshes[i];
            for(duint i = 0; i < mesh.mNumVertices; ++i)
            {
                addToBounds(Vector3f(&mesh.mVertices[i].x));
            }
        }

        // Initialize all meshes in the scene into a single GL buffer.
        makeBuffer();

        // Print some information.
        qDebug() << "total bones:" << boneCount();

        // Animations.
        qDebug() << "animations:" << scene->mNumAnimations;
        for(duint i = 0; i < scene->mNumAnimations; ++i)
        {
            qDebug() << "  anim #" << i << "name:" << scene->mAnimations[i]->mName.C_Str();
        }

        // Materials.
        qDebug() << "materials:" << scene->mNumMaterials;
        for(duint i = 0; i < scene->mNumMaterials; ++i)
        {
            aiMaterial const &mat = *scene->mMaterials[i];
            qDebug() << "  material #" << i
                     << "texcount (diffuse):" << mat.GetTextureCount(aiTextureType_DIFFUSE);

            aiString texPath;
            for(duint s = 0; s < mat.GetTextureCount(aiTextureType_DIFFUSE); ++s)
            {
                if(mat.GetTexture(aiTextureType_DIFFUSE, s, &texPath) == AI_SUCCESS)
                {
                    qDebug() << "    texture #" << s << texPath.C_Str();
                }
            }
        }
    }

    void addToBounds(Vector3f const &point)
    {
        minPoint = minPoint.min(point);
        maxPoint = maxPoint.max(point);
    }

    // Bone & Mesh Setup -----------------------------------------------------

    int boneCount() const
    {
        return bones.size();
    }

    int addBone(String const &name)
    {
        int idx = boneCount();
        bones << BoneData();
        boneNameToIndex[name] = idx;
        return idx;
    }

    int findBone(String const &name) const
    {
        if(boneNameToIndex.contains(name))
        {
            return boneNameToIndex[name];
        }
        return -1;
    }

    int addOrFindBone(String const &name)
    {
        int i = findBone(name);
        if(i >= 0)
        {
            return i;
        }
        return addBone(name);
    }

    void addVertexWeight(duint vertexIndex, duint16 boneIndex, dfloat weight)
    {
        VertexBone &vb = vertexBones[vertexIndex];
        for(int i = 0; i < MAX_BONES_PER_VERTEX; ++i)
        {
            if(vb.weights[i] == 0.f)
            {
                // Here's a free one.
                vb.ids[i] = boneIndex;
                vb.weights[i] = weight;
                return;
            }
        }
        DENG2_ASSERT(!"Too many bone weights for a vertex");
    }

    /**
     * Initializes the per-vertex bone weight information, and indexes the bones
     * of the mesh in a sequential order.
     *
     * @param mesh        Source mesh.
     * @param vertexBase  Index of the first vertex of the mesh.
     */
    void initBones(aiMesh const &mesh, duint vertexBase)
    {
        vertexBones.resize(vertexBase + mesh.mNumVertices);

        for(duint i = 0; i < mesh.mNumBones; ++i)
        {
            aiBone const &bone = *mesh.mBones[i];

            duint const boneIndex = addOrFindBone(bone.mName.C_Str());
            bones[boneIndex].offset = convertMatrix(bone.mOffsetMatrix);

            for(duint w = 0; w < bone.mNumWeights; ++w)
            {
                addVertexWeight(vertexBase + bone.mWeights[w].mVertexId,
                                boneIndex, bone.mWeights[w].mWeight);
            }
        }
    }

    VBuf *makeBuffer()
    {
        VBuf::Vertices verts;
        VBuf::Indices indx;

        aiVector3D const zero(0, 0, 0);

        // All of the scene's meshes are combined into one GL buffer.
        for(duint m = 0; m < scene->mNumMeshes; ++m)
        {
            aiMesh const &mesh = *scene->mMeshes[m];

            qDebug() << "initializing mesh:" << mesh.mName.C_Str();
            qDebug() << "  material:" << mesh.mMaterialIndex;
            qDebug() << "  bones:" << mesh.mNumBones;

            int vtxBase = vertexBones.size();

            initBones(mesh, vtxBase);

            // Load vertices into the buffer.
            for(duint i = 0; i < mesh.mNumVertices; ++i)
            {
                aiVector3D const *pos      = &mesh.mVertices[i];
                aiVector3D const *normal   = (mesh.HasNormals()? &mesh.mNormals[i] : &zero);
                aiVector3D const *texCoord = (mesh.HasTextureCoords(0)? &mesh.mTextureCoords[0][i] : &zero);
                aiVector3D const *tangent  = (mesh.HasTangentsAndBitangents()? &mesh.mTangents[i] : &zero);
                aiVector3D const *bitang   = (mesh.HasTangentsAndBitangents()? &mesh.mBitangents[i] : &zero);

                VBuf::Type v;
                v.pos       = Vector3f(pos->x, pos->y, pos->z);
                v.normal    = Vector3f(normal->x, normal->y, normal->z);
                v.tangent   = Vector3f(tangent->x, tangent->y, tangent->z);
                v.bitangent = Vector3f(bitang->x, bitang->y, bitang->z);
                v.texCoord  = Vector2f(texCoord->x, texCoord->y);

                for(int b = 0; b < MAX_BONES_PER_VERTEX; ++b)
                {
                    v.boneIds[b]     = vertexBones[vtxBase + i].ids[b];
                    v.boneWeights[b] = vertexBones[vtxBase + i].weights[b];
                }

                verts << v;
            }

            // Get face indices.
            for(duint i = 0; i < mesh.mNumFaces; ++i)
            {
                aiFace const &face = mesh.mFaces[i];
                DENG2_ASSERT(face.mNumIndices == 3); // expecting triangles
                indx << face.mIndices[0] + vtxBase
                     << face.mIndices[1] + vtxBase
                     << face.mIndices[2] + vtxBase;
            }
        }

        buffer = new VBuf;
        buffer->setVertices(verts, gl::Static);
        buffer->setIndices(gl::Triangles, indx, gl::Static);

        qDebug() << "new GLbuf" << buffer << "verts:" << verts.size();
        return buffer;
    }

    // Animation -------------------------------------------------------------

    struct AccumData
    {
        ddouble time;
        aiAnimation const *anim;
        QVector<Matrix4f> finalTransforms;

        AccumData(int boneCount)
            : time(0)
            , anim(0)
            , finalTransforms(boneCount)
        {}

        aiNodeAnim const *findNodeAnim(aiNode const &node) const
        {
            for(duint i = 0; i < anim->mNumChannels; ++i)
            {
                aiNodeAnim const *na = anim->mChannels[i];
                if(na->mNodeName == node.mName)
                {
                    return na;
                }
            }
            return 0;
        }
    };

    void accumulateAnimationTransforms(ddouble time,
                                       aiAnimation const &anim,
                                       aiNode const &rootNode)
    {
        ddouble const ticksPerSec = anim.mTicksPerSecond? anim.mTicksPerSecond : 25.0;
        ddouble const timeInTicks = time * ticksPerSec;

        AccumData data(boneCount());
        data.anim = &anim;
        data.time = std::fmod(timeInTicks, anim.mDuration);

        accumulateTransforms(rootNode, data);

        // Update the resulting matrices in the uniform.
        for(int i = 0; i < boneCount(); ++i)
        {
            uBoneMatrices.set(i, data.finalTransforms.at(i));
        }
    }

    void accumulateTransforms(aiNode const &node, AccumData &data,
                              Matrix4f const &parentTransform = Matrix4f()) const
    {
        Matrix4f nodeTransform = convertMatrix(node.mTransformation);

        if(aiNodeAnim const *anim = data.findNodeAnim(node))
        {
            // Interpolate for this point in time.
            Matrix4f const translation = Matrix4f::translate(interpolatePosition(data.time, *anim));
            Matrix4f const scaling     = Matrix4f::scale(interpolateScaling(data.time, *anim));
            Matrix4f const rotation    = convertMatrix(aiMatrix4x4(interpolateRotation(data.time, *anim).GetMatrix()));

            nodeTransform = translation * rotation * scaling;
        }

        Matrix4f globalTransform = parentTransform * nodeTransform;

        int boneIndex = findBone(String(node.mName.C_Str()));
        if(boneIndex >= 0)
        {
            data.finalTransforms[boneIndex] =
                    globalInverse * globalTransform * bones.at(boneIndex).offset;
        }

        // Descend to child nodes.
        for(duint i = 0; i < node.mNumChildren; ++i)
        {
            accumulateTransforms(*node.mChildren[i], data, globalTransform);
        }
    }

    template <typename Type>
    static duint findAnimKey(ddouble time, Type const *keys, duint count)
    {
        DENG2_ASSERT(count > 0);
        for(duint i = 0; i < count - 1; ++i)
        {
            if(time < keys[i + 1].mTime)
            {
                return i;
            }
        }
        DENG2_ASSERT(!"Failed to find animation key (invalid time?)");
        return 0;
    }

    static Vector3f interpolateVectorKey(ddouble time, aiVectorKey const *keys, duint at)
    {
        Vector3f const start(&keys[at]    .mValue.x);
        Vector3f const end  (&keys[at + 1].mValue.x);

        return start + (end - start) *
               float((time - keys[at].mTime) / (keys[at + 1].mTime - keys[at].mTime));
    }

    static aiQuaternion interpolateRotation(ddouble time, aiNodeAnim const &anim)
    {
        if(anim.mNumRotationKeys == 1)
        {
            return anim.mRotationKeys[0].mValue;
        }

        aiQuatKey const *key = anim.mRotationKeys + findAnimKey(time, anim.mRotationKeys, anim.mNumRotationKeys);;

        aiQuaternion interp;
        aiQuaternion::Interpolate(interp, key[0].mValue, key[1].mValue,
                                  (time - key[0].mTime) / (key[1].mTime - key[0].mTime));
        interp.Normalize();
        return interp;
    }

    static Vector3f interpolateScaling(ddouble time, aiNodeAnim const &anim)
    {
        if(anim.mNumScalingKeys == 1)
        {
            return Vector3f(&anim.mScalingKeys[0].mValue.x);
        }
        return interpolateVectorKey(time, anim.mScalingKeys,
                                    findAnimKey(time, anim.mScalingKeys,
                                                anim.mNumScalingKeys));
    }

    static Vector3f interpolatePosition(ddouble time, aiNodeAnim const &anim)
    {
        if(anim.mNumPositionKeys == 1)
        {
            return Vector3f(&anim.mPositionKeys[0].mValue.x);
        }
        return interpolateVectorKey(time, anim.mPositionKeys,
                                    findAnimKey(time, anim.mPositionKeys,
                                                anim.mNumPositionKeys));
    }

    // Drawing ---------------------------------------------------------------

    void draw()
    {
        DENG2_ASSERT(program != 0);
        DENG2_ASSERT(buffer != 0);

        // Draw the meshes in this node.
        if(scene->HasAnimations())
        {
            accumulateAnimationTransforms(animTime, *scene->mAnimations[0],
                                          *scene->mRootNode);
        }

        GLState::current().apply();

        program->beginUse();
        buffer->draw();
        program->endUse();
    }
};

ModelDrawable::ModelDrawable() : d(new Instance(this))
{
    *this += d->modelAsset;
}

void ModelDrawable::load(File const &file)
{
    LOG_AS("ModelDrawable");

    // Get rid of all existing data.
    clear();

    d->import(file);
}

void ModelDrawable::clear()
{
    glDeinit();
    d->clear();
}

void ModelDrawable::glInit()
{
    d->glInit();
}

void ModelDrawable::glDeinit()
{
    d->glDeinit();
}

void ModelDrawable::setProgram(GLProgram &program)
{
    d->program = &program;
    d->program->bind(d->uBoneMatrices);
}

void ModelDrawable::unsetProgram()
{
    if(d->program)
    {
        d->program->unbind(d->uBoneMatrices);
    }

    d->program = 0;
}

void ModelDrawable::setAnimationTime(TimeDelta const &time)
{
    d->animTime = time;
}

void ModelDrawable::draw()
{
    glInit();
    if(isReady() && d->program)
    {
        d->draw();
    }
}

Vector3f ModelDrawable::dimensions() const
{
    return d->maxPoint - d->minPoint;
}

Vector3f ModelDrawable::midPoint() const
{
    return (d->maxPoint + d->minPoint) / 2.f;
}

} // namespace de
