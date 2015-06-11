
#ifndef __THER_BUZZER_H__
#define __THER_BUZZER_H__

enum {
	BUZZER_MUSIC_SYS_BOOT = 0,
	BUZZER_MUSIC_SEND_TEMP,
	BUZZER_MUSIC_WARNING,

	BUZZER_MUSIC_NR,
};

void ther_buzzer_init(unsigned char task_id);
void ther_buzzer_play_music(void);
void ther_play_music(unsigned char music);
void ther_stop_music(void);

#endif

