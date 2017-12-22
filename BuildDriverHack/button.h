/*
 * button.h
 *
 * Created: 2017/02/22 0:20:32
 *  Author: T.Yamaguchi
 */ 


#ifndef BUTTON_H_
#define BUTTON_H_


enum event_type
{
	Press,
	Release,
	Repeat
};

typedef struct BUTTON_T
{
	uint8_t id;
	uint16_t press_counter;
	uint16_t release_counter;
	void (*func)( struct BUTTON_T *btn, uint8_t event ) ;
	// 
	union {
		uint8_t flags;
		struct {
			uint8_t disabled : 1;
			uint8_t pressed : 1;
		};
	};
} BUTTON_T;

void BUTTON_Init( BUTTON_T *btn, uint8_t id, void (*func)( struct BUTTON_T *btn, uint8_t event )  )
{
	btn->id = id;
	btn->func = func;
}

void BUTTON_Update( BUTTON_T *btn, uint8_t pressed )
{
	if( pressed ){
		if( btn->press_counter < UINT16_MAX ){
			btn->press_counter++;
		}
		if( btn->press_counter == 100 ){
			btn->pressed = 1;
			btn->release_counter = 0;
			btn->func(btn,Press);
		}
	}else{
		if( btn->release_counter < UINT16_MAX ){
			btn->release_counter++;
		}
		if( btn->release_counter == 100 ){
			btn->pressed = 0;
			btn->press_counter = 0;
			btn->func(btn,Release);
		}
	}
}

#endif /* BUTTON_H_ */