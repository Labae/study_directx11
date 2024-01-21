float4 VS(float4 Pos : POSITION) : SV_Position
{
    return Pos;
}

float4 PS(float4 Pos : SV_Position) : SV_Target
{
    return float4(1.0f, 0.0f, 0.0f, 1.0f);
}