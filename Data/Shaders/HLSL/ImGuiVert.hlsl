struct VertexInput
{
    float2 position : POSITION;
    float2 texCoords : TEXCOORD0;
    float4 colour : COLOR0;
};

struct VertexOutput
{
    float4 position : SV_Position;
    float2 texCoords : TEXCOORD0;
    float4 colour : COLOR0;
};

struct Constants
{
    float2 translation;
    float2 scale;
};

ConstantBuffer<Constants> rootConstants : register(b0, space0);

VertexOutput main(VertexInput input)
{
    VertexOutput output;
    output.position = float4((input.position * rootConstants.scale) + rootConstants.translation, 0.0f, 1.0f);
    output.colour = input.colour;
    output.texCoords = input.texCoords;

    return output;
}
