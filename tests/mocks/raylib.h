#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Vector2 { float x; float y; } Vector2;

Vector2 GetMouseDelta(void);
void DisableCursor(void);
void EnableCursor(void);
void ShowCursor(void);

#ifdef __cplusplus
}
#endif
