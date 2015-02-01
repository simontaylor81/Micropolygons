//--------------------------------------------------------------------------------------
// Rasterization effects.
//--------------------------------------------------------------------------------------

struct VS_IN
{
	float4 Pos : POSITION;
	half4 Colour : COLOR;
	float4 Eqn_As : TEXCOORD0;
	float4 Eqn_Bs : TEXCOORD1;
	float4 Eqn_Cs : TEXCOORD2;
};

struct PS_IN
{
	half4 Colour : COLOR;
	float4 Eqn_As : TEXCOORD0;
	float4 Eqn_Bs : TEXCOORD1;
	float4 Eqn_Cs : TEXCOORD2;
};

float4 VS(VS_IN In, out PS_IN Out) : SV_POSITION
{
	Out.Colour = In.Colour;
	Out.Eqn_As = In.Eqn_As;
	Out.Eqn_Bs = In.Eqn_Bs;
	Out.Eqn_Cs = In.Eqn_Cs;
	return In.Pos;
}

float4 PS(float4 Pos : SV_POSITION, PS_IN In) : SV_Target
{
	clip(In.Eqn_As * Pos.xxxx + In.Eqn_Bs * Pos.yyyy - In.Eqn_Cs);

	return In.Colour;
	//return In.Eqn_As;
}

// Technique Definition
technique10 Render
{
	pass P0
	{
		SetVertexShader( CompileShader(vs_4_0, VS()) );
		SetGeometryShader( NULL );
		SetPixelShader( CompileShader(ps_4_0, PS()) );
	}
}
