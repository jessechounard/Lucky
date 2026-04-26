// Depth-only fragment shader. The pipeline targets a depth attachment
// with no color attachment, so SV_Target0 is unused; we still emit it
// because some backends require the fragment shader to declare an output
// matching what's bound at draw time (none here, but a zero output is
// harmless).

struct VSOutput {
    float4 Position : SV_Position;
};

float4 main(VSOutput input) : SV_Target0 {
    return float4(0, 0, 0, 0);
}
