//--------------------------------------------------------------------------------------
// Shader resources
//--------------------------------------------------------------------------------------

Texture2D   g_Diffuse; // Material albedo for diffuse lighting


//--------------------------------------------------------------------------------------
// Constant buffers
//--------------------------------------------------------------------------------------


cbuffer cbChangesEveryFrame
{
    matrix  g_WorldViewProjection;
    int		g_FrameIndex;
};


//--------------------------------------------------------------------------------------
// Structs
//--------------------------------------------------------------------------------------


//--------------------------------------------------------------------------------------
// Rasterizer states
//--------------------------------------------------------------------------------------

RasterizerState rsDefault {
};

RasterizerState rsCullFront {
    CullMode = Front;
};

RasterizerState rsCullBack {
    CullMode = Back;
};

RasterizerState rsCullNone {
	CullMode = None; 
};

RasterizerState rsLineAA {
	CullMode = None; 
	AntialiasedLineEnable = true;
};


//--------------------------------------------------------------------------------------
// DepthStates
//--------------------------------------------------------------------------------------
DepthStencilState EnableDepth
{
    DepthEnable = TRUE;
    DepthWriteMask = ALL;
    DepthFunc = LESS_EQUAL;
};

BlendState NoBlending
{
    AlphaToCoverageEnable = FALSE;
    BlendEnable[0] = FALSE;
};



// Calculates vertex positions for a single triangle in the xz-Plane
float4 TriPosFromIndex(uint id)
{
	switch (id)
	{
		case 0: return float4(-1, 0, -1, 1);
		case 1: return float4(1, 0, 1, 1);
		case 2: return float4(1, 0, -1, 1);
		default: return 0;
	}
}

//--------------------------------------------------------------------------------------
// Shaders: Simple, fixed color
//--------------------------------------------------------------------------------------

float4 SimpleVS(uint id: SV_VertexID): SV_Position 
{
	float4 pos = TriPosFromIndex(id);

    // Transform position from object space to homogenious clip space
	return mul(pos, g_WorldViewProjection);
}

float4 SimplePS(float4 pos: SV_Position) : SV_Target0 
{
	return float4(1, 1, 1, 1);
}


//--------------------------------------------------------------------------------------
// Shaders: Vertex colors calculated by position
//--------------------------------------------------------------------------------------

struct ColorPSIn
{
	float4 pos: SV_Position;
	float4 col: Color;
};


ColorPSIn ColorVS(in uint id: SV_VertexID)
{
	ColorPSIn result = (ColorPSIn)0;

	float4 pos = TriPosFromIndex(id);
	result.col = pos / 2 + 0.5f;

    // Transform position from object space to homogenious clip space
	result.pos = mul(pos, g_WorldViewProjection);

	return result;
}

float4 ColorPS(ColorPSIn frag) : SV_Target0 
{
	return frag.col;
}


//--------------------------------------------------------------------------------------
// Shaders: Position altered by current frame index
//--------------------------------------------------------------------------------------


void SineMovementVS(uint id: SV_VertexID, out float4 col: Color, out float4 position: SV_Position) 
{
	float4 pos = TriPosFromIndex(id);
	col = pos / 2 + 0.5f;
	pos.y = sin(pos.x * 2.0f + (float)g_FrameIndex / 1000.0f);

    // Transform position from object space to homogenious clip space
	position = mul(pos, g_WorldViewProjection);
}

float4 SineMovementPS(float4 col: Color, float4 pos: SV_Position) : SV_Target0 
{
	return col;
}


//--------------------------------------------------------------------------------------
// Shaders: Color change in pixel shader
//--------------------------------------------------------------------------------------


void SineColorVS(uint id: SV_VertexID, out float4 worldPos: WorldPos, out float4 position: SV_Position) 
{
	worldPos = TriPosFromIndex(id);

    // Transform position from object space to homogenious clip space
	position = mul(worldPos, g_WorldViewProjection);
}

float4 SineColorPS(float4 worldPos: WorldPos, float4 pos: SV_Position) : SV_Target0 
{
	return float4(sin(worldPos.x / 2 * 3.14f + (float)g_FrameIndex / 1000.0f), 1, 0, 1);
}




//--------------------------------------------------------------------------------------
// Techniques
//--------------------------------------------------------------------------------------
technique11 Render
{
    pass Simple
    {
        SetVertexShader(CompileShader(vs_4_0, SimpleVS()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_4_0, SimplePS()));
        
        SetRasterizerState(rsCullNone);
        SetDepthStencilState(EnableDepth, 0);
        SetBlendState(NoBlending, float4(0.0f, 0.0f, 0.0f, 0.0f), 0xFFFFFFFF);
    }

	pass Color
    {
        SetVertexShader(CompileShader(vs_4_0, ColorVS()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_4_0, ColorPS()));
        
        SetRasterizerState(rsCullNone);
        SetDepthStencilState(EnableDepth, 0);
        SetBlendState(NoBlending, float4(0.0f, 0.0f, 0.0f, 0.0f), 0xFFFFFFFF);
    }

	pass SineMovement
    {
        SetVertexShader(CompileShader(vs_4_0, SineMovementVS()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_4_0, SineMovementPS()));
        
        SetRasterizerState(rsCullNone);
        SetDepthStencilState(EnableDepth, 0);
        SetBlendState(NoBlending, float4(0.0f, 0.0f, 0.0f, 0.0f), 0xFFFFFFFF);
    }

	pass SineColor
    {
        SetVertexShader(CompileShader(vs_4_0, SineColorVS()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_4_0, SineColorPS()));
        
        SetRasterizerState(rsCullNone);
        SetDepthStencilState(EnableDepth, 0);
        SetBlendState(NoBlending, float4(0.0f, 0.0f, 0.0f, 0.0f), 0xFFFFFFFF);
    }
}
