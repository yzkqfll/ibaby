
#ifndef __THER_BUZZER_H__
#define __THER_BUZZER_H__

enum {
	BUZZER_MUSIC_SYS_POWER_ON = 0,
	BUZZER_MUSIC_SYS_POWER_OFF,
	BUZZER_MUSIC_SEND_TEMP,
	BUZZER_MUSIC_HIGH_TEMP_WARNING,
	BUZZER_MUSIC_LOW_BATT_WARNING,

	BUZZER_MUSIC_NR,
};

void ther_buzzer_init(unsigned char task_id);
void ther_buzzer_exit(void);
void ther_buzzer_playing_music(void);
void ther_buzzer_start_music(unsigned char music);
void ther_buzzer_stop_music(void);

#endif

