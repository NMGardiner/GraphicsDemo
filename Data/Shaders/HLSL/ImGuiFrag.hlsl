struct VertexOutput
{
    float4 position : SV_Position;
    float2 texCoords : TEXCOORD0;
    float4 colour : COLOR0;
}; 

SamplerState fontSampler : register(s0, space0);
Texture2D fontTexture : register(t0, space0);

float4 main(VertexOutput input) : SV_Target0
{
    return float4(input.colour * fontTexture.Sample(fontSampler, input.texCoords));
}
