//==================================================================
/// ImmGL.h
///
/// Created by Davide Pasca - 2022/05/26
/// See the file "license.txt" that comes with this project for
/// copyright info.
//==================================================================

#ifndef IMMGL_H
#define IMMGL_H

#include <array>
#include <vector>
#include <string>
#include <memory>
#include <unordered_map>
#include "MathBase.h"

template <typename T> using IVec = std::vector<T>;

using IFloat2 = glm::vec2;
using IFloat3 = glm::vec3;
using IFloat4 = glm::vec4;
using IMat4   = glm::mat4;
using IColor4 = glm::vec4;

using IUInt = unsigned int;
using IStr  = std::string;

//==================================================================
class ShaderProg
{
public:
    IUInt   mProgramID  {};

    std::unordered_map<IStr,IUInt>  mLocs;

    ShaderProg( bool useTex );
    ~ShaderProg();

    const auto GetProgramID() const { return mProgramID; }

    void SetUniform( const char *pName, float v );
    void SetUniform( const char *pName, int v );
    void SetUniform( const char *pName, const IMat4 &m );
    void SetUniform( const char *pName, const IFloat3 &v );
    void SetUniform( const char *pName, const IFloat4 &v );
private:
    IUInt getLoc( const char *pName );
};

//==================================================================
class ImmGL
{
    struct VtxPC
    {
        IFloat3 pos;
        IColor4 col;
    };
    struct VtxPCT
    {
        IFloat3 pos;
        IColor4 col;
        IFloat2 tc0;
    };
    IVec<VtxPC>     mVtxPC;
    IVec<VtxPCT>    mVtxPCT;

    enum : int
    {
        BM_NONE,
        BM_ADD,
        BM_ALPHA,
    };
    int             mCurBlendMode = BM_NONE;

    enum : IUInt
    {
        FLG_LINES = 1 << 0,
        FLG_TEX   = 1 << 1,
    };
    IUInt           mModeFlags = 0;
    IUInt           mCurTexID = 0;
    ShaderProg      *mpCurShaProg   {};
    IMat4           mCurMtxPS {1.f};

    IVec<std::unique_ptr<ShaderProg>>   moShaProgs;

    IUInt           mVAO = 0;
    IUInt           mVBO = 0;
    size_t          mLastVBOSize {};

public:
    ImmGL();

    void ResetStates();
    void FlushPrims();

    void SetBlendNone();
    void SetBlendAdd();
    void SetBlendAlpha();

    void SetTexture( IUInt texID );
    void SetNoTexture() { SetTexture( 0 ); }

    void SetMtxPS( const IMat4 &m );

    void DrawLine( const IFloat3 &p1, const IFloat3 &p2, const IColor4 &col );
    void DrawLine( const IFloat3 &p1, const IFloat3 &p2, const IColor4 &col1, const IColor4 &col2 );

    void DrawQuad( const std::array<IFloat3,4> &poss, const std::array<IColor4,4> &cols );
    void DrawQuad( const std::array<IFloat3,4> &poss, const IColor4 &col );

    template <typename PT>
    void DrawRectFill( const PT &pos, const IFloat2 &siz, const std::array<IColor4,4> &cols );
    template <typename PT>
    void DrawRectFill( const PT &pos, const IFloat2 &siz, const IColor4 &col );

    void DrawRectFill( const IFloat4 &rc, const IColor4 &col )
    {
        DrawRectFill( IFloat2{rc[0],rc[1]}, IFloat2{rc[2],rc[3]}, col );
    }

private:
    //==================================================================
    template <typename T> static inline void resize_loose( IVec<T> &vec, size_t newSize )
    {
        if ( newSize > vec.capacity() )
        {
            auto locmax = []( auto a, auto b ) { return a > b ? a : b; };
            vec.reserve( locmax( newSize, vec.capacity()/2*3 ) );
        }
        vec.resize( newSize );
    }
    //
    template <typename T> static inline T *growVec( IVec<T> &vec, size_t growN )
    {
        size_t n = vec.size();
        resize_loose<T>( vec, n + growN );
        return vec.data() + n;
    }

    std::array<IFloat3,4> makeRectVtxPos( const IFloat2 &pos, const IFloat2 &siz ) const
    {
        return { IFloat3{pos[0]+siz[0]*0, pos[1]+siz[1]*0, 0},
                 IFloat3{pos[0]+siz[0]*1, pos[1]+siz[1]*0, 0},
                 IFloat3{pos[0]+siz[0]*0, pos[1]+siz[1]*1, 0},
                 IFloat3{pos[0]+siz[0]*1, pos[1]+siz[1]*1, 0} };
    }
    std::array<IFloat3,4> makeRectVtxPos( const IFloat3 &pos, const IFloat2 &siz ) const
    {
        return { IFloat3{pos[0]+siz[0]*0, pos[1]+siz[1]*0, pos[2]},
                 IFloat3{pos[0]+siz[0]*1, pos[1]+siz[1]*0, pos[2]},
                 IFloat3{pos[0]+siz[0]*0, pos[1]+siz[1]*1, pos[2]},
                 IFloat3{pos[0]+siz[0]*1, pos[1]+siz[1]*1, pos[2]} };
    }

    void switchModeFlags( IUInt flags );

    //==================================================================
    template <typename D, typename M, typename S>
    static void setQuadStripAsTrigs(
            D out[6], M member, const S &v0, const S &v1, const S &v2, const S &v3 )
    {
        out[0].*member = v0;
        out[1].*member = v1;
        out[2].*member = v2;
        out[3].*member = v3;
        out[4].*member = v2;
        out[5].*member = v1;
    }

    template <typename D, typename S>
    static void setQuadStripAsTrigsP(D out[6],const S &v0,const S &v1,const S &v2,const S &v3)
    {
        setQuadStripAsTrigs( out, &D::pos, v0, v1, v2, v3 );
    }
    template <typename D, typename S>
    static void setQuadStripAsTrigsC(D out[6],const S &v0,const S &v1,const S &v2,const S &v3)
    {
        setQuadStripAsTrigs( out, &D::col, v0, v1, v2, v3 );
    }
    template <typename D, typename S>
    static void setQuadStripAsTrigsT(D out[6],const S &v0,const S &v1,const S &v2,const S &v3)
    {
        setQuadStripAsTrigs( out, &D::tc0, v0, v1, v2, v3 );
    }
};

//==================================================================
inline void ImmGL::DrawLine(
        const IFloat3 &p1,
        const IFloat3 &p2,
        const IColor4 &col )
{
    switchModeFlags( FLG_LINES );

    auto *pVtx = growVec( mVtxPC, 2 );
    pVtx[0].pos = { p1[0], p1[1], 0 };
    pVtx[1].pos = { p2[0], p2[1], 0 };

    pVtx[0].col =
    pVtx[1].col = col;
}


//==================================================================
inline void ImmGL::DrawLine(
        const IFloat3 &p1,
        const IFloat3 &p2,
        const IColor4 &col1,
        const IColor4 &col2 )
{
    switchModeFlags( FLG_LINES );

    auto *pVtx = growVec( mVtxPC, 2 );
    pVtx[0].pos = { p1[0], p1[1], 0 };
    pVtx[1].pos = { p2[0], p2[1], 0 };

    pVtx[0].col = col1;
    pVtx[1].col = col2;
}

//==================================================================
inline void ImmGL::DrawQuad(
            const std::array<IFloat3,4> &poss,
            const std::array<IColor4,4> &cols )
{
    switchModeFlags( 0 );
    auto *pVtx = growVec( mVtxPC, 6 );
    setQuadStripAsTrigsP( pVtx, poss[0], poss[1], poss[2], poss[3] );
    setQuadStripAsTrigsC( pVtx, cols[0], cols[1], cols[2], cols[3] );
}

//==================================================================
inline void ImmGL::DrawQuad(
            const std::array<IFloat3,4> &poss,
            const IColor4 &col )
{
    switchModeFlags( 0 );
    auto *pVtx = growVec( mVtxPC, 6 );
    setQuadStripAsTrigsP( pVtx, poss[0], poss[1], poss[2], poss[3] );
    setQuadStripAsTrigsC( pVtx, col, col, col, col );
}

//==================================================================
template <typename PT>
inline void ImmGL::DrawRectFill(
            const PT &pos,
            const IFloat2 &siz,
            const std::array<IColor4,4> &cols )
{
    DrawQuad( makeRectVtxPos( pos, siz ), cols );
}

//==================================================================
template <typename PT>
inline void ImmGL::DrawRectFill(
            const PT &pos,
            const IFloat2 &siz,
            const IColor4 &col )
{
    DrawQuad( makeRectVtxPos( pos, siz ), col );
}

#endif

