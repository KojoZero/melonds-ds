#version 140
layout(std140) uniform uConfig
{
    vec2 uScreenSize;
    uint u3DScale;
    uint uFilterMode;
    vec4 cursorPos;   // left, top, right, bottom in normalized coords
    bool cursorVisible;
};

uniform sampler2D ScreenTex;

smooth in vec2 fTexcoord;
out vec4 oColor;

void main()
{
    vec4 pixel = texture(ScreenTex, fTexcoord);

    // Only draw on bottom screen half
    if (cursorVisible && fTexcoord.y >= 0.5 && fTexcoord.y <= 1.0)
    {
        // Half-open box test to avoid double-including far edges
        bool inside =
            (fTexcoord.x >= cursorPos.x) && (fTexcoord.x < cursorPos.z) &&
            (fTexcoord.y >= cursorPos.y) && (fTexcoord.y < cursorPos.w);

        if (inside)
        {
            // Normalize to [0,1] inside the rect
            float w = max(cursorPos.z - cursorPos.x, 1e-6);
            float h = max(cursorPos.w - cursorPos.y, 1e-6);
            float u = clamp((fTexcoord.x - cursorPos.x) / w, 0.0, 1.0);
            float v = clamp((fTexcoord.y - cursorPos.y) / h, 0.0, 1.0);

            // Bucket into a 5×5 grid (use tiny eps to avoid boundary ties)
            const float eps = 1e-6;
            int bx = clamp(int(floor((u * 5.0) - eps)), 0, 4);
            int by = clamp(int(floor((v * 5.0) - eps)), 0, 4);

            // Classify: white 3×3 center, black outer ring (no corners)
            int dx = bx - 2;
            int dy = by - 2;
            int adx = abs(dx);
            int ady = abs(dy);

            bool inWhite3x3 = (adx <= 1 && ady <= 1);

            bool onOuterRow = (by == 0 || by == 4);
            bool onOuterCol = (bx == 0 || bx == 4);
            bool isCorner   = onOuterRow && onOuterCol;

            bool inRing = !isCorner && (
                (onOuterRow && adx <= 1) ||
                (onOuterCol && ady <= 1)
            );

            if (inWhite3x3)
                pixel.rgb = vec3(1.0);  // white fill
            else if (inRing)
                pixel.rgb = vec3(0.0);  // black outline (corners skipped)
            // else: leave underlying pixel
        }
    }

    // Your pipeline outputs BGR with forced alpha 1.0
    oColor = vec4(pixel.bgr, 1.0);
}
