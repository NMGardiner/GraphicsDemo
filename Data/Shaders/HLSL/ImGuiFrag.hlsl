struct VertexOutput
{
    float4 position : SV_Position;
    float2 texCoords : TEXCOORD0;
    float4 colour : COLOR0;
}; 

struct Constants
{
    uint textureIndex;
};

ConstantBuffer<Constants> rootConstants : register(b0, space0);

SamplerState textureSampler : register(s0, space0);
Texture2D textures[64] : register(t0, space0);

float4 main(VertexOutput input) : SV_Target0
{
    return float4(input.colour * textures[NonUniformResourceIndex(rootConstants.textureIndex)].Sample(textureSampler, input.texCoords));
}
