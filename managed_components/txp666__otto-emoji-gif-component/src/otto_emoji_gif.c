/**
 * @file otto_emoji_gif.c
 * @brief Otto robot GIF emoji component - minimal API, content is gifs/ folder
 * only
 */

#include "otto_emoji_gif.h"
#include <stddef.h>

#ifndef OTTO_EMOJI_GIF_VERSION
#define OTTO_EMOJI_GIF_VERSION "1.1.1"
#endif

static const char *const gif_names[] = {
    "anger", "anger_talking", "angry", "angry_talking",
    "buxue", "buxue_talking", "confident", "confident_talking",
    "confused", "confused_talking", "cool", "cool_talking",
    "crying", "crying_talking", "delicious", "delicious_talking",
    "embarrassed", "embarrassed_talking", "funny", "funny_talking",
    "happy", "happy_talking", "idle", "idle_talking",
    "kissy", "kissy_talking", "laughing", "laughing_talking",
    "loving", "loving_talking", "neutral", "neutral_talking",
    "relaxed", "relaxed_talking", "sad", "sad_talking",
    "scare", "scare_talking", "shocked", "shocked_talking",
    "silly", "silly_talking", "sleepy", "sleepy_talking",
    "smirk", "smirk_talking", "staticstate", "staticstate_talking",
    "surprised", "surprised_talking", "thinking", "thinking_talking",
    "winking", "winking_talking",
    NULL};

const char *otto_emoji_gif_get_version(void) { return OTTO_EMOJI_GIF_VERSION; }

int otto_emoji_gif_get_count(void) {
  int count = 0;
  while (gif_names[count] != NULL) {
    count++;
  }
  return count;
}

const char *otto_emoji_gif_get_name(int index) {
  if (index < 0 || index >= otto_emoji_gif_get_count()) {
    return NULL;
  }
  return gif_names[index];
}
