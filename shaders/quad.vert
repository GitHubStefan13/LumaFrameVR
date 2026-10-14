#version 450
// One full-screen triangle for both the video quad and the pointer overlay. They differ only in
// depth: the video sits at the far plane and the pointer just in front of it. That depth is a
// specialization constant, so each pipeline states it and there is one source file instead of two
// that were the same trick with a different literal.
layout(constant_id = 0) const float quad_depth = 0.0;
layout(location = 0) out vec2 position;
void main() {
    position = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2) * 2.0 - 1.0;
    gl_Position = vec4(position, quad_depth, 1.0);
}
