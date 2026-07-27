struct Particle {
    float2 position;
    float2 velocity;
    float4 color;
};
struct ParticleSSBO {
    Particle particles;
};

struct UniformBuffer {
    float delta_time;
};

ConstantBuffer<UniformBuffer> ubo;
StructuredBuffer<ParticleSSBO> particles_in;
RWStructuredBuffer<ParticleSSBO> particles_out;

[shader("compute")]
[numthreads(256, 1, 1)]
void compute_main(uint3 thread_id: SV_DispatchThreadID) {
    uint index = thread_id.x;

    particles_out[index].particles.position =
        particles_in[index].particles.position + particles_in[index].particles.velocity.xy * ubo.delta_time;
    particles_out[index].particles.velocity = particles_in[index].particles.velocity;

    // Flip movement at window border
    if ((particles_out[index].particles.position.x <= -1.0) ||
        (particles_out[index].particles.position.x >= 1.0)
    ) {
        particles_out[index].particles.velocity.x = -particles_out[index].particles.velocity.x;
    }
    if ((particles_out[index].particles.position.y <= -1.0) ||
        (particles_out[index].particles.position.y >= 1.0)
    ) {
        particles_out[index].particles.velocity.y = -particles_out[index].particles.velocity.y;
    }
}

struct VertexInput {
    float2 in_position;
    float4 in_color;
};

struct VertexOutput {
    float4 sv_position: SV_Position;
    float point_size: SV_PointSize;
    float3 frag_color: COLOR0;
};

struct FragmentInput {
    float4 position: SV_Position;
    float3 frag_color: COLOR0;
    float2 point_coord: SV_PointCoord;
};

[shader("vertex")]
VertexOutput vertex_main(VertexInput input) {
    VertexOutput output;
    output.point_size = 14.0;
    output.sv_position = float4(input.in_position, 1.0, 1.0);
    output.frag_color = input.in_color.rgb;
    return output;
}

[shader("fragment")]
float4 fragment_main(FragmentInput input): SV_TARGET {
    float2 coord = input.point_coord - float2(0.5);
    return float4(input.frag_color, 0.5 - length(coord));
}
