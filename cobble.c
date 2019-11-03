#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/uinput.h>

#define EV_MAKE   1
#define EV_BREAK  0
#define EV_REPEAT 2

#define MOUSE_UP 0
#define MOUSE_DOWN 1
#define MOUSE_SCROLL 2

struct input_event event;
int keyboard_fd, mouse_fd;

typedef struct _action {
	char *name;
	int (*callback)(float);
	struct _action *next;
} action;

action *actions;

action *CreateAction(char *name, int (*callback)(float))
{
	action *a = (action *)malloc(sizeof(action));
	if(a == NULL)
	{
		printf("Unable to allocate memory\n");
		exit(1);
	}
	
	a->name = (char *)malloc(sizeof(char) * strlen(name));
	strcpy(a->name, name);
	
	a->callback = callback;
	a->next = NULL;
	
	return a;
}

void AddAction(char *name, int (*callback)(float))
{
	if(actions == NULL)
	{
		actions = CreateAction(name, callback);
		return;
	}
	
	action *c = actions;
	
	while(c->next != NULL)
	{
		c = c->next;
	}
	
	c->next = CreateAction(name, callback);
}

int DoActions(char *name, float fElapsedTime)
{
	int ret = 0;
	
	action *c = actions;
	while(c != NULL)
	{
		if(strcmp(c->name, name) == 0)
		{
			ret += (*c->callback)(fElapsedTime);
		}
		c = c->next;
	}
	
	return ret;
}

void Emit(int fd, int t, int c, int v)
{
	struct input_event ie;
	
	ie.type = t; ie.code = c;
	ie.value = v; ie.time.tv_sec = 0;
	ie.time.tv_usec = 0;
	
	write(fd, &ie, sizeof(ie));
}

void Mouse(int button, int d)
{
	
	if(button == MOUSE_UP || button == MOUSE_DOWN)
	{
		Emit(mouse_fd, EV_KEY, BTN_LEFT, button);
		Emit(mouse_fd, EV_SYN, SYN_REPORT, 0);
	}
	
	if(button == MOUSE_SCROLL)
	{
		Emit(mouse_fd, EV_REL, REL_WHEEL, d);
		Emit(mouse_fd, EV_SYN, SYN_REPORT, 0);
	}
	
}

int KeyUp(int key)
{
	return (event.code == key && event.type == EV_KEY && event.value == EV_BREAK) ? 1 : 0;
}

void Run(char *kb_device)
{
	struct timeval t1, t2;
	struct uinput_setup usetup;
	
	keyboard_fd = open(kb_device, O_RDONLY | O_NONBLOCK);
	if(keyboard_fd == -1)
	{
		printf("Cannot Open Keyboard: %s\n", strerror(errno));
		exit(1);
	}

	mouse_fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
	if(mouse_fd == -1)
	{
		printf("Cannot Open Mouse: %s\n", strerror(errno));
		exit(1);
	}
	
	ioctl(mouse_fd, UI_SET_EVBIT, EV_KEY);
	ioctl(mouse_fd, UI_SET_KEYBIT, BTN_LEFT);

	ioctl(mouse_fd, UI_SET_EVBIT, EV_REL);
	ioctl(mouse_fd, UI_SET_RELBIT, REL_WHEEL);

	
	memset(&usetup, 0, sizeof(usetup));
	usetup.id.bustype = BUS_USB;
	usetup.id.vendor = 0x1234;
	usetup.id.product = 0x5678;
	strcpy(usetup.name, "CobbleFarmerMouse");
	
	ioctl(mouse_fd, UI_DEV_SETUP, &usetup);
	ioctl(mouse_fd, UI_DEV_CREATE);
	
	float fElapsedTime;
	
	gettimeofday(&t1, NULL);
	int flag = 0;
	
	while(flag == 0)
	{
		gettimeofday(&t2, NULL);
		fElapsedTime = (float)((double)(t2.tv_usec - t1.tv_usec) / 1000000 + (double)(t2.tv_sec - t1.tv_sec));
		t1 = t2;
	
		read(keyboard_fd, &event, sizeof(event));
		
		flag = DoActions("main", fElapsedTime);
		usleep(1000000/60);
	}
	
	close(keyboard_fd);
	
	ioctl(mouse_fd, UI_DEV_DESTROY);
	close(mouse_fd);
}

float fDelay = 200.0f;
float fDelayTracker = 0.0f;
float fFrameTracker = 0.0f;
int nFPS = 0;
int nPickAxeCounter;
int running = 0;

void DoNextPickaxe()
{
	Mouse(MOUSE_UP, 0);
	Mouse(MOUSE_SCROLL, -1);
	Mouse(MOUSE_DOWN, 0);
}

int CobbleFarmer(float fElapsedTime)
{
	if(KeyUp(KEY_Z) == 1)
	{
		if(running == 0)
		{
			running = 1;
			fDelayTracker = 0.0f;
			nPickAxeCounter = 1;
			Mouse(MOUSE_DOWN, 0);
		}
		else
		{
			running = 0;
			Mouse(MOUSE_UP, 0);
		}
	}
	
	if(KeyUp(KEY_DELETE))
		return 1;
	
	if(running == 1)
	{
		if(nPickAxeCounter > 9)
		{
			running = 0;
			Mouse(MOUSE_UP, 0);
			return 0;
		}
		
		fDelayTracker += fElapsedTime;
		if(fDelayTracker > fDelay)
		{
			DoNextPickaxe();
			fDelayTracker -= fDelay;
			nPickAxeCounter++;
		}
	}
	
	return 0;
	
}

int CobbleFarmerOutput(float fElapsedTime)
{
	fFrameTracker += fElapsedTime;
	nFPS++;
	
	if(fFrameTracker > 1.0f)
	{
		fFrameTracker -= 1.0f;
		printf("\033]0;%cMoros Cobblestone Farmer - %d FPS%c%c", 0, nFPS, 7, 0);
		nFPS = 0;
	}
	
	printf("\033c");
	printf("Moros Cobblestone Farmer\n");
	printf("-----------------------------------------\n");
	printf("Running:                %d\n", running);
	printf("Delay Between Pickaxes: %f\n", fDelay);
	printf("Delay Tracker:          %f\n", fDelayTracker);
	printf("Current Pickaxe:        %d\n", nPickAxeCounter);
	
	return 0;
}

int main(void)
{
	actions = NULL;

	AddAction("main", &CobbleFarmer);
	AddAction("main", &CobbleFarmerOutput);

	Run("/dev/input/event5");
	
	return 0;
}
