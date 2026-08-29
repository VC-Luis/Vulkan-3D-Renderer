struct VSInput
{
    float3 inPosition;
    float3 inColor;
    float2 inTexCoord;
};

struct VSOutput
{
    float4 pos : SV_Position;
    float3 fragColor;
    float2 fragTexCoord;
};

struct UniformBuffer
{
    float4x4 modelMatrix;
    float4x4 viewMatrix;
    float4x4 projectionMatrix;
};

ConstantBuffer<UniformBuffer> UBO;
Sampler2D texture;

[shader("vertex")]
VSOutput vertMain(VSInput input)
{
    VSOutput output;
    output.pos = mul(UBO.projectionMatrix, mul(UBO.viewMatrix, mul(UBO.modelMatrix, float4(input.inPosition, 1.0))));
    output.fragColor = input.inColor;
    output.fragTexCoord = input.inTexCoord;
    return output;
}

[shader("fragment")]
float4 fragMain(VSOutput vertIn) : SV_Target
{
    return float4(vertIn.fragColor * texture.Sample(vertIn.fragTexCoord).rgb, 1.0);
    //return float4(vertIn.fragTexCoord, 0.0, 1.0);
}