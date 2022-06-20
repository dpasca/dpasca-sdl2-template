//==================================================================
/// ImmGL.cpp
///
/// Created by Davide Pasca - 2022/05/26
/// See the file "license.txt" that comes with this project for
/// copyright info.
//==================================================================

#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include <stdexcept>
#include <GL/glew.h>
#include "ImmGL.h"

#undef  c_auto
#undef  NOT
#undef  __DSHORT_FILE__

#define c_auto const auto
#define NOT(X) (!(X))

//==================================================================
inline const char *DEX_SeekFilename( const char *pStr )
{
    return
        (strrchr(pStr,'/') ? strrchr(pStr,'/')+1 : \
            (strrchr(pStr,'\\') ? strrchr(pStr,'\\')+1 : pStr) );
}

//==================================================================
#define __DSHORT_FILE__ DEX_SeekFilename( __FILE__ )

//==================================================================
inline std::string DEX_MakeString( const char *pFmt, ... )
{
    constexpr size_t BUFF_LEN = 2048;

    char buff[ BUFF_LEN ];
    va_list args;
    va_start( args, pFmt );
    vsnprintf( buff, sizeof(buff), pFmt, args );
    va_end(args);

    buff[ BUFF_LEN-1 ] = 0;

    return buff;
}

# define DEX_RUNTIME_ERROR(_FMT_,...) \
    throw std::runtime_error( \
        DEX_MakeString( "[%s:%i] " _FMT_, __DSHORT_FILE__, __LINE__, ##__VA_ARGS__ ) )

//==================================================================
#if defined(DEBUG) || defined(_DEBUG)
# define CHECKGLERR CheckGLErr(__FILE__,__LINE__)
# define FLUSHGLERR FlushGLErr()
#else
# define CHECKGLERR
# define FLUSHGLERR
#endif

//==================================================================
static const char *getErrStr( GLenum err )
{
    switch ( err )
    {
    case GL_NO_ERROR:           return "GL_NO_ERROR";
    case GL_INVALID_ENUM:       return "GL_INVALID_ENUM";
    case GL_INVALID_VALUE:      return "GL_INVALID_VALUE";
    case GL_INVALID_OPERATION:  return "GL_INVALID_OPERATION";
    case GL_OUT_OF_MEMORY:      return "GL_OUT_OF_MEMORY";
    case GL_INVALID_FRAMEBUFFER_OPERATION: return "GL_INVALID_FRAMEBUFFER_OPERATION";
    default:
        {
            static char buff[128] {};
            sprintf( buff, "#x%04x", err );
            return buff;
        }
    }
}

//==================================================================
static bool CheckGLErr( const char *pFileName, int line )
{
    bool didErr = false;
	GLenum err = glGetError();
	while (err != GL_NO_ERROR)
	{
        didErr = true;
        const char *pErrStr = getErrStr( err );

        if ( pErrStr )
            printf( "GL error: %s at %s : %i", pErrStr, pFileName, line );
        else
            printf( "Unknown error: %d 0x%x at %s : %i", err, err, pFileName, line );

		err = glGetError();
	}

    return didErr;
}

//==================================================================
static void FlushGLErr()
{
	while ( glGetError() != GL_NO_ERROR )
    {
    }
}

//==================================================================
static void check_shader_compilation( GLuint oid, bool isLink )
{
    GLint n {};
    if ( isLink )
        glGetProgramiv( oid, GL_LINK_STATUS, &n );
    else
        glGetShaderiv( oid, GL_COMPILE_STATUS, &n );

    if NOT( n )
    {
        if ( isLink )
            glGetProgramiv( oid, GL_INFO_LOG_LENGTH, &n );
        else
            glGetShaderiv( oid, GL_INFO_LOG_LENGTH, &n );

        IVec<GLchar>  info_log( n );

        if ( isLink )
            glGetProgramInfoLog( oid, n, &n, info_log.data() );
        else
            glGetShaderInfoLog( oid, n, &n, info_log.data() );

        DEX_RUNTIME_ERROR(
                "%s %s failed: %*s",
                isLink ? "Program" : "Shader",
                isLink ? "linking" : "compilation",
                n,
                info_log.data() );
    }
}

//==================================================================
ShaderProg::ShaderProg( bool useTex )
{
static const IStr vtxSrouce = R"RAW(
// input attributes
layout (location = 0) in vec3 a_pos;
layout (location = 1) in vec4 a_col;
#ifdef USE_TEX
layout (location = 2) in vec2 a_tc0;
#endif

// out varyings
out vec4 v_col;
#ifdef USE_TEX
out vec2 v_tc0;
#endif

// vertx main
void main()
{
   v_col = a_col;
#ifdef USE_TEX
   v_tc0 = a_tc0;
#endif
   gl_Position = u_mtxPS * vec4( a_pos, 1.0 );
}
)RAW";

//---------------
static const IStr frgSource = R"RAW(
// input varyings
in vec4 v_col;
#ifdef USE_TEX
in vec2 v_tc0;
#endif

#ifdef USE_TEX
#endif

// out color
out vec4 o_col;

// fragment main
void main()
{
   o_col = v_col
#ifdef USE_TEX
            * texture( s_tex, v_tc0 )
#endif
            ;
}
)RAW";

    // header
    IStr header = R"RAW(
#version 330

uniform mat4x4  u_mtxPS;
uniform sampler2D s_tex;
)RAW";

    if ( useTex )
        header += "#define USE_TEX\n";

    auto makeShader = [&]( c_auto type, const IStr &src )
    {
        c_auto obj = glCreateShader( type );

        c_auto fullStr = header + src;
        const GLchar *ppsrcs[2] = { fullStr.c_str(), 0 };

        glShaderSource( obj, 1, &ppsrcs[0], nullptr ); CHECKGLERR;
        glCompileShader( obj ); CHECKGLERR;

        check_shader_compilation( obj, false );
        return obj;
    };

    c_auto shaderVtx = makeShader( GL_VERTEX_SHADER  , vtxSrouce );
    c_auto shaderFrg = makeShader( GL_FRAGMENT_SHADER, frgSource );

    mProgramID = glCreateProgram();

    glAttachShader( mProgramID, shaderVtx );
    glAttachShader( mProgramID, shaderFrg );
    glLinkProgram( mProgramID );

    check_shader_compilation( mProgramID, true );

    // always detach and delete shaders after a successful link
    glDetachShader( mProgramID, shaderVtx );
    glDetachShader( mProgramID, shaderFrg );
    glDeleteShader( shaderVtx );
    glDeleteShader( shaderFrg );
}

//==================================================================
ShaderProg::~ShaderProg()
{
    if ( mProgramID )
        glDeleteProgram( mProgramID );
}

//==================================================================
IUInt ShaderProg::getLoc( const char *pName )
{
    auto it = mLocs.find( pName );
    if ( it == mLocs.end() )
        it = mLocs.insert({ pName, glGetUniformLocation( mProgramID, pName ) }).first;

    return it->second;
}

void ShaderProg::SetUniform( const char *pName, float v )
{
    glUniform1f( getLoc( pName ), v );
}
void ShaderProg::SetUniform( const char *pName, int v )
{
    glUniform1i( getLoc( pName ), v );
}
void ShaderProg::SetUniform( const char *pName, const IMat4 &m )
{
    glUniformMatrix4fv( getLoc( pName ), 1, false, (const float *)&m );
}
void ShaderProg::SetUniform( const char *pName, const IFloat3 &v )
{
    glUniform3fv( getLoc( pName ), 1, (const float *)&v[0] );
}
void ShaderProg::SetUniform( const char *pName, const IFloat4 &v )
{
    glUniform4fv( getLoc( pName ), 1, (const float *)&v[0] );
}

//==================================================================
//==================================================================
ImmGL::ImmGL()
{
    FLUSHGLERR;

    moShaProgs.push_back( std::make_unique<ShaderProg>( false ) );
    moShaProgs.push_back( std::make_unique<ShaderProg>( true ) );

    glGenBuffers( 1, &mVBO );
    CHECKGLERR;

    glGenVertexArrays( 1, &mVAO );
    CHECKGLERR;
}

//==================================================================
void ImmGL::SetBlendNone()
{
    if ( mCurBlendMode == BM_NONE )
        return;

    mCurBlendMode = BM_NONE;
    FlushPrims();

    glDisable( GL_BLEND );
}

//==================================================================
void ImmGL::SetBlendAdd()
{
    if ( mCurBlendMode == BM_ADD )
        return;

    mCurBlendMode = BM_ADD;
    FlushPrims();

    glEnable( GL_BLEND );
    glBlendFunc( GL_SRC_ALPHA, GL_ONE );
}

//==================================================================
void ImmGL::SetBlendAlpha()
{
    if ( mCurBlendMode == BM_ALPHA )
        return;

    mCurBlendMode = BM_ALPHA;
    FlushPrims();

    glEnable( GL_BLEND );
    glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
}

//==================================================================
void ImmGL::SetTexture( IUInt texID )
{
    if ( mCurTexID == texID )
        return;

    FlushPrims();
    mCurTexID = texID;
}

//==================================================================
void ImmGL::SetMtxPS( const IMat4 &m )
{
    if ( mCurMtxPS == m )
        return;

    FlushPrims();
    mCurMtxPS = m;
}

//==================================================================
void ImmGL::switchModeFlags( IUInt flags )
{
    if ( mModeFlags == flags )
        return;

    FlushPrims();
    mModeFlags = flags;
}

//==================================================================
void ImmGL::ResetStates()
{
    glDisableClientState( GL_VERTEX_ARRAY );
    glDisableClientState( GL_COLOR_ARRAY );
    glDisableClientState( GL_TEXTURE_COORD_ARRAY );

    mCurBlendMode = BM_NONE;
    glDisable( GL_BLEND );

    mModeFlags = 0;

    glDisable( GL_TEXTURE_2D );
    mCurTexID = (IUInt)0;
    glBindTexture( GL_TEXTURE_2D, mCurTexID );

    //
    glBindBuffer( GL_ARRAY_BUFFER, 0 );

    //
    mpCurShaProg = nullptr;
    glUseProgram( 0 );

    //
    glBindVertexArray( 0 );
}

//==================================================================
void ImmGL::FlushPrims()
{
    c_auto n = (GLsizei)((mModeFlags & FLG_TEX) ? mVtxPCT.size() : mVtxPC.size());
    if NOT( n )
        return;

    FLUSHGLERR;
    c_auto newVBOSize  = (mModeFlags & FLG_TEX)
                        ? mVtxPCT.size() * sizeof(mVtxPCT[0])
                        : mVtxPC.size()  * sizeof(mVtxPC[0]);

    glBindBuffer( GL_ARRAY_BUFFER, mVBO ); CHECKGLERR;

    // expand as necessary
    if ( mLastVBOSize != newVBOSize )
    {
        mLastVBOSize = newVBOSize;
        glBufferData( GL_ARRAY_BUFFER, newVBOSize, 0, GL_DYNAMIC_DRAW );
        CHECKGLERR;
    }

    if ( (mModeFlags & FLG_TEX) )
        glBufferSubData( GL_ARRAY_BUFFER, 0, newVBOSize, mVtxPCT.data() );
    else
        glBufferSubData( GL_ARRAY_BUFFER, 0, newVBOSize, mVtxPC.data() );
    CHECKGLERR;

    //
    glBindVertexArray( mVAO );

    glEnableVertexAttribArray( 0 ); CHECKGLERR;
    glEnableVertexAttribArray( 1 ); CHECKGLERR;
    if ( (mModeFlags & FLG_TEX) )
    {
        auto vp = []( c_auto idx, c_auto cnt, c_auto off )
        {
            glVertexAttribPointer(
                idx, cnt, GL_FLOAT, GL_FALSE, sizeof(VtxPCT), (const void *)off );
            CHECKGLERR;
        };
        vp( 0, 3, offsetof(VtxPCT,pos) );
        vp( 1, 4, offsetof(VtxPCT,col) );

        glEnableVertexAttribArray( 2 ); CHECKGLERR;
        vp( 2, 2, offsetof(VtxPCT,tc0) );

        //
        glActiveTexture( GL_TEXTURE0 ); CHECKGLERR;
        glBindTexture( GL_TEXTURE_2D, mCurTexID ); CHECKGLERR;
    }
    else
    {
        auto vp = []( c_auto idx, c_auto cnt, c_auto off )
        {
            glVertexAttribPointer(
                idx, cnt, GL_FLOAT, GL_FALSE, sizeof(VtxPC), (const void *)off );
            CHECKGLERR;
        };
        vp( 0, 3, offsetof(VtxPC,pos) );
        vp( 1, 4, offsetof(VtxPC,col) );

        glDisableVertexAttribArray( 2 ); CHECKGLERR;
    }

    //
    auto *pProg = moShaProgs[(mModeFlags & FLG_TEX) ? 1 : 0].get();
    if ( mpCurShaProg != pProg )
    {
        mpCurShaProg = pProg;
        glUseProgram( pProg->GetProgramID() ); CHECKGLERR;
    }

    // set the uniforms
    pProg->SetUniform( "u_mtxPS", mCurMtxPS );

    //
    glDrawArrays( (mModeFlags & FLG_LINES) ? GL_LINES : GL_TRIANGLES, 0, n );
    CHECKGLERR;

    //glUseProgram( 0 );
    glBindVertexArray( 0 );

    glBindBuffer( GL_ARRAY_BUFFER, 0 );
    CHECKGLERR;

    mVtxPC.clear();
    mVtxPCT.clear();
}

//
#undef  c_auto
#undef  NOT
#undef  __DSHORT_FILE__

