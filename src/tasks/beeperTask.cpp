#include <Arduino.h>

#include "config.h"
#include "pitches.h"
#include "button.h"
#include "beeperTask.h"
#include "utils.h"

//
// ring melody
//

int ringMelody[] = {
	NOTE_C7, NOTE_D7, NOTE_E7, NOTE_F7};

int ringDurations[] = {
	30, 10, 30, 10};

#define NUM_RING_REPEATS 50

//
// alarm signals
//

// 2 short beeps per second
int melody1[] = {
	BEEP_NOTE, -1, BEEP_NOTE, -1};

// 4 short beeps per second
int melody2[] = {
	BEEP_NOTE, -1, BEEP_NOTE, -1, BEEP_NOTE, -1, BEEP_NOTE, -1};

// note durations: 4 = quarter note, 8 = eighth note, etc.:
int noteDurations1[] = {
	40, 40, 40, 840};

int noteDurations2[] = {
	40, 40, 40, 40, 40, 40, 40, 720};

#define NUM_RING_NOTES (sizeof(ringMelody) / sizeof(ringMelody[0]))
#define NUM_RING_DURATIONS (sizeof(ringDurations) / sizeof(ringDurations[0]))

#define NUM_NOTES1 (sizeof(melody1) / sizeof(melody1[0]))
#define NUM_DURATIONS1 (sizeof(noteDurations1) / sizeof(noteDurations1[0]))

#define NUM_NOTES2 (sizeof(melody2) / sizeof(melody2[0]))
#define NUM_DURATIONS2 (sizeof(noteDurations2) / sizeof(noteDurations2[0]))

#define NUM_MELODIES 2

#define SPEED 1.0

class BeeperContext {
public:
	bool m_alarmRunning = false;
	bool m_bellRunning = false;

	unsigned int m_melodyStart = 0;
	unsigned int m_melodyPos = 0;
	unsigned int m_notePos = 0;
	int m_repeatCnt = 0;

	bool m_bellOn;
	bool m_alarmOn;

	Button m_bellButton;
	
	BeeperContext()
	: m_bellOn(false)
	, m_alarmOn(false)
	, m_bellButton(INPUT_BELL_PIN, true, 10)
	{
	}

	void init()
	{
		// make sure durations/notes structures match
		assert(NUM_RING_NOTES == NUM_RING_DURATIONS);
		assert(NUM_NOTES1 == NUM_DURATIONS1);
		assert(NUM_NOTES2 == NUM_DURATIONS2);

		// apply song speed
		for (int i = 0; i < NUM_RING_NOTES; i++) {
			ringDurations[i] = ringDurations[i] / SPEED;
		}

		for (int i = 0; i < NUM_NOTES1; i++) {
			noteDurations1[i] = noteDurations1[i] / SPEED;
		}

		for (int i = 0; i < NUM_NOTES2; i++) {
			noteDurations2[i] = noteDurations2[i] / SPEED;
		}

		// configure buzzer for output
		pinMode(BUZZER_PIN, OUTPUT);

		// disable beep
		setNote(-1);
	}
	
	void alarmOn(const bool &on)
	{
		m_alarmOn = on;
	}

	void bellOn(const bool &on)
	{
		m_bellOn = on;
	}

	void setNote(int note)
	{
		static int lastNote = -1;

		if (note != lastNote) {
			LOG_PRINTF("Setting note %d\n", note);
			lastNote = note;

			if (note < 0) {
				ledcDetachPin(BUZZER_PIN);
			} else {
				ledcAttachPin(BUZZER_PIN, BUZZER_PWM_CHANNEL);
				ledcWriteTone(BUZZER_PWM_CHANNEL, note);
			}
		}
	}

	void processTone()
	{
		//
		// are bell / alarm buttons pressed?
		//

		bool bellPressed = false;
		bool alarmPressed = false;

		// check if the bell is active
		m_bellButton.read();
		if (m_bellButton.isPressed()) {
			bellPressed = true;
		} else {
			bellPressed = false;
		}

		alarmPressed |= m_alarmOn;
		bellPressed |= m_bellOn;

		static bool prevBellPressed = false;
		static bool prevAlarmPressed = false;

		if (bellPressed != prevBellPressed)
		{
			prevBellPressed = bellPressed;
			LOG_PRINTF("Bell button %s\n", bellPressed ? "pressed" : "released");
		}

		if (alarmPressed != prevAlarmPressed)
		{
			prevAlarmPressed = alarmPressed;
			LOG_PRINTF("Alarm button %s\n", alarmPressed ? "pressed" : "released");
		}

		//
		// is the alarm button pressed?
		//

		if (alarmPressed)
		{
			alarmButtonPressed();
		}

		//
		// is the bell button pressed?
		//

		else if (bellPressed)
		{
			bellButtonPressed();
		}

		//
		// no melody running, clear note
		//

		else
		{
			m_bellRunning = false;
			m_alarmRunning = false;
			setNote(-1);
		}
	}

	void bellButtonPressed()
	{
		// shall we start a new melody?
		if (!m_bellRunning) {

			//
			// new melody sequence initialization
			//

			m_bellRunning = true;
			m_melodyStart = millis();
			m_notePos = 0;
			m_melodyPos = 0;
			m_repeatCnt = 0;
		}

	repeat:
		//
		// get the current melody
		//

		int *melody = ringMelody;
		int *noteDurations = ringDurations;
		int numNotes = NUM_RING_NOTES;

		// have we played everything?
		if ((m_notePos >= numNotes) || (m_repeatCnt >= NUM_RING_REPEATS)) {
			// final state - muted
			setNote(-1);
			return;
		}

		// get current relative time
		unsigned int timeSinceStart = millis() - m_melodyStart;

		// switch to a next note if needed
		if (timeSinceStart > noteDurations[m_notePos]) {

			// switch to the next note
			m_notePos++;
			m_melodyStart = millis();

			// have we played all notes? then jump to the next sequence
			if (m_notePos >= numNotes)
			{

				// increment repeat counter
				m_repeatCnt++;

				// start from first note again
				m_notePos = 0;

				// if we run through all repeats, leave
				if (m_repeatCnt >= NUM_RING_REPEATS)
				{
					goto repeat;
				}
			}
		}

		// set the tone for this note
		setNote(melody[m_notePos]);
	}

	void alarmButtonPressed()
	{
		// shall we start a new melody?
		if (!m_alarmRunning) {

			//
			// new melody sequence initialization
			//

			m_alarmRunning = true;
			m_melodyStart = millis();
			m_notePos = 0;
			m_melodyPos = 0;
			m_repeatCnt = 0;
		}

	repeat:
		//
		// get the current melody
		//

		int *melody = NULL;
		int *noteDurations = NULL;
		int numNotes = 0;

		if (m_melodyPos == 0) {
			melody = melody1;
			noteDurations = noteDurations1;
			numNotes = NUM_NOTES1;
		} else {
			melody = melody2;
			noteDurations = noteDurations2;
			numNotes = NUM_NOTES2;
		}

		// have we played everything?
		if ((m_notePos >= numNotes) || (m_repeatCnt >= NUM_REPEATS) || (m_melodyPos >= NUM_MELODIES)) {
			// final state - beeping forever
			setNote(BEEP_NOTE);
			return;
		}

		// get current relative time
		unsigned int timeSinceStart = millis() - m_melodyStart;

		// switch to a next note if needed
		if (timeSinceStart > noteDurations[m_notePos]) {

			// switch to the next note
			m_notePos++;
			m_melodyStart = millis();

			// have we played all notes? then jump to the next sequence
			if (m_notePos >= numNotes) {

				// increment repeat counter
				m_repeatCnt++;

				// start from first note again
				m_notePos = 0;

				// if we run through all repeats, leave
				if (m_repeatCnt >= NUM_REPEATS) {

					// jump to the next melody
					m_melodyPos++;
					m_repeatCnt = 0;
					goto repeat;
				}
			}
		}

		// set the tone for this note
		setNote(melody[m_notePos]);
	}

	void task()
	{
		init();

		while (1) {
			processTone();
			delay(10);
		}
	}
};

static BeeperContext g_ctx;

void beeperTask(void *pvParameters __attribute__((unused)))
{
	g_ctx.task();
}

void beeperAlarmOn(const bool &on)
{
	g_ctx.alarmOn(on);
}

void beeperBellOn(const bool &on)
{
	g_ctx.bellOn(on);
}

