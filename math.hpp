#ifndef MATH_HPP
#define MATH_HPP

#include <cmath>

#include "client.hpp"
#include "hacks/player_info.hpp"
#include "engine.hpp"

#include "memory.hpp"
#include "vector.hpp"

//180/pi
#define radpi 57.295779513082f

//pi/180
#define pideg 0.017453293f

#define isNaN(n) n != n

inline static float distance_3d(Vector3 location_one, Vector3 location_two) {
  return sqrt(((location_one.y - location_two.y)*(location_one.y - location_two.y)) +
	      ((location_one.x - location_two.x)*(location_one.x - location_two.x)) +
	      ((location_one.z - location_two.z)*(location_one.z - location_two.z)));
}
 
inline static float vmatrix[4][4];

/* The game does not keep a world to screen matrix around any more, so it is
   built from the camera of the local player: the eye angles, the origin of
   the pawn and the field of view. */
inline static void build_view_matrix() {
  const float pitch = PlayerInfo::local_view_angles[0] * (float)(M_PI / 180.0);
  const float yaw = PlayerInfo::local_view_angles[1] * (float)(M_PI / 180.0);

  const float cp = cosf(pitch);
  const float forward[3] = {cp * cosf(yaw), cp * sinf(yaw), sinf(pitch)};

  float right[3] = {forward[1], -forward[0], 0.0f};
  const float right_len = sqrtf(right[0] * right[0] + right[1] * right[1]);
  if (right_len > 0.0001f) {
    right[0] /= right_len;
    right[1] /= right_len;
  }

  const float up[3] = {right[1] * forward[2] - right[2] * forward[1],
                       right[2] * forward[0] - right[0] * forward[2],
                       right[0] * forward[1] - right[1] * forward[0]};

  const float eye[3] = {PlayerInfo::local_origin[0], PlayerInfo::local_origin[1],
                        PlayerInfo::local_origin[2] + 62.0f};

  const float width = (float)Engine::window_width;
  const float height = (float)Engine::window_height;
  const float fov_x = 90.0f * (float)(M_PI / 180.0);
  float fov_y = 2.0f * atanf(tanf(fov_x / 2.0f) * (height > 1.0f ? height / width : 1.0f));

  const float scale_x = 1.0f / tanf(fov_x / 2.0f);
  const float scale_y = 1.0f / tanf(fov_y / 2.0f);

  const float dot_right = right[0] * eye[0] + right[1] * eye[1] + right[2] * eye[2];
  const float dot_up = up[0] * eye[0] + up[1] * eye[1] + up[2] * eye[2];
  const float dot_forward = forward[0] * eye[0] + forward[1] * eye[1] + forward[2] * eye[2];

  vmatrix[0][0] = right[0] * scale_x;
  vmatrix[0][1] = right[1] * scale_x;
  vmatrix[0][2] = right[2] * scale_x;
  vmatrix[0][3] = -dot_right;

  vmatrix[1][0] = up[0] * scale_y;
  vmatrix[1][1] = up[1] * scale_y;
  vmatrix[1][2] = up[2] * scale_y;
  vmatrix[1][3] = -dot_up;

  vmatrix[2][0] = forward[0];
  vmatrix[2][1] = forward[1];
  vmatrix[2][2] = forward[2];
  vmatrix[2][3] = -dot_forward;

  vmatrix[3][0] = forward[0];
  vmatrix[3][1] = forward[1];
  vmatrix[3][2] = forward[2];
  vmatrix[3][3] = dot_forward;
}
inline static bool world_to_screen(pid_t gamePid, const Vector3 vIn, float vOut[2], bool ignore_oof=true) {
  build_view_matrix();

  float w = vmatrix[3][0] * vIn.x + vmatrix[3][1] * vIn.y + vmatrix[3][2] * vIn.z + vmatrix[3][3];

  if (w < 0.1f) return false;

  float vOutTmp[2];
  
  vOutTmp[0] = vmatrix[0][0] * vIn.x + vmatrix[0][1] * vIn.y + vmatrix[0][2] * vIn.z + vmatrix[0][3];
  vOutTmp[1] = vmatrix[1][0] * vIn.x + vmatrix[1][1] * vIn.y + vmatrix[1][2] * vIn.z + vmatrix[1][3];
  float invw = 1.0f / w;

  vOutTmp[0] *= invw;
  vOutTmp[1] *= invw;

  float x = Engine::window_width / 2.0f;
  float y = Engine::window_height / 2.0f;

  x += 0.5f * vOutTmp[0] * Engine::window_width + 0.5f;
  y -= 0.5f * vOutTmp[1] * Engine::window_height + 0.5f;
  
  vOut[0] = x * 100.f / 100.f;
  vOut[1] = y * 100.f / 100.f;

  if (ignore_oof == true) {
    if (vOut[0] > Engine::window_width*2 || vOut[1] > Engine::window_height*2 || vOut[0] < -Engine::window_width || vOut[1] < -Engine::window_height) return false;
  }
  
  return true;
}

#endif
