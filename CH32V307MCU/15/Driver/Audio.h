#ifndef DRIVER_AUDIO_H_
#define DRIVER_AUDIO_H_

void Usart6_Init(void);
void audio_init();
void audio_play(u8 num);
void BusyPin_Init(void);
u8 IsAudioBusy(void);

#endif /* DRIVER_AUDIO_H_ */

/******
1.请选择支付方式
2.支付成功
3.
 *
 */
