#include <stdlib.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <wiringPi.h>

#ifdef __cplusplus
extern "C"{
#endif
typedef uint8_t boolean;
typedef uint8_t byte;

#if !defined(NULL)
#endif
#ifdef __cplusplus
}
#endif



unsigned long mClock=540;
unsigned long mClockTolerance=30;
long mIIRFactor=8;

int mRxPin;

unsigned long mLastLow; // time of previous pulse
unsigned long mAvgLength256;
int mAvgLength;
unsigned long mOrigin;
long mAvgPhase256=0;
long mAvgPhase=0;
int mStarted=0;

float mBitsAtOrigin=0.0;
float mLastBits=0.0;
float mCurrentBitAnalog=0.0;
int j=0;

unsigned long nowDummy=10000;
void processBits(int v, unsigned long now, int N);
void capture(int v, unsigned long now);
void lw_setup(int aPin);

int mBreak;
int mACId;
int mACSub;
int mACCmd;
int mACParam;
int mACErrors=1000;
unsigned long mACTime;

void runTest();
void runLive();

int main(int argc, char *argv[])
{
	if (argc != 3) {
		printf("usage: %s GPIOPin duration\n", argv[0]);
		return 2;
  	}
  	int gpio = atoi(argv[1]);
  	unsigned long duration = atol(argv[2])*1000;
  	
	//printf("listening on GPIO %d\n",gpio);
	runLive(gpio,duration);
	//runTest();
	return 0;
}

void runLive(int gpio, unsigned long duration)
{
	mAvgLength256=mClock*mIIRFactor;
	mAvgLength=mClock;
	
	lw_setup(gpio);
	unsigned long now=micros();
	unsigned long start=now;
	mOrigin=now;
	
	int lastId=0, lastSub=0, lastCmd=0, lastParam=0;
	while (micros()-start<duration)
	{	usleep(200000); //200ms
		now=micros();
		if (mACId!=lastId || mACSub!=lastSub || mACCmd!=lastCmd || mACParam!=lastParam )
		{	
			if (now-mACTime>500000 || mACErrors==0) //1 second
			{
				printf("%08d,%02d,%02d,%02d,%03d\n",mACId,mACSub,mACCmd,mACParam,mACErrors);
				fflush(stdout);
				lastId=mACId; 
				lastSub=mACSub;
				lastCmd=mACCmd;
				lastParam=mACParam;
				mACTime=now;
				return;
			}
		}
		if (lastId!=0 && now-mACTime>500000)
		{	lastId=lastSub=lastCmd=lastParam=0;
			mACId=mACSub=mACCmd=mACParam=0;
			mACErrors=1000;
			//printf("reset\n");
		}
	}
}

void runTest()
{
	int i=0;
	unsigned long clock=528;
	for (i=0; i<20; i++)
	{
		capture(1,clock);
		clock+=250;
		capture(0,clock);
		if ((i%4)==0)
		{	clock+=1334;
		}
		else
		{	clock+=278;
		}
	}
	printf("\n");
}



int n;
unsigned long avgDur;
long mAvgProx=0;
unsigned long mLastLow;
float mPeriod=528.0;
float mError=0.0;
float mAbsError=0.0;
float mAvgPos=0.0;
float mAvgDev=0.0;
float fa=0.95;
float fb=0.05;
unsigned long mOffset=10000;
float mEdgePeriod=528.0;
void processBits(int v, unsigned long now, int N)
{
	if (v==0)
	{
		unsigned long d=now-mLastLow;
		mLastLow=now;
		float cycles=((float)d)/mPeriod;
		float icycles=round(cycles);
		float error=cycles-icycles;
		float fabsError=fabs(error);
		mAbsError=mAbsError*fa+fabsError*fb;
		if (fabsError<0.1)
		{	mError=mError*fa+error*fb;
			
			//this works with low noise
			mPeriod+=mError*30.0;
		}
		if (mAbsError>0.3)
		{	//bad;
			mPeriod=528.0;
		}
		
		//average position of edge within period
		unsigned long per=(unsigned long)(mPeriod+0.5);
		unsigned long p=now-mOrigin;
		unsigned long c=p/per;
		unsigned long offset=c*per;
		mOrigin+=offset;
		p-=offset;
		float r=((float)p)/mPeriod;
		mAvgPos=mAvgPos*0.9+r*0.1;
		float dev=fabs(mAvgPos-r);
		mAvgDev=mAvgDev*fa+dev*fb;
		
		if (mAvgPos>0.5 && mAvgPos<0.9)
		{	mOrigin+=1;
		}
		else if (mAvgPos<0.5 && mAvgPos>0.1)
		{	mOrigin-=1;
		}
		
		mEdgePeriod=mEdgePeriod*fa+((float)d)*fb;
		
		if (n++>500)
		{	n=0;
			//if (mAbsError<0.1)
			{
				printf("err=%1.2f dev=%0.2f period=%.2f phase=%.2f ep=%0.1f\n",mAbsError,mAvgDev,mPeriod,mAvgPos,mEdgePeriod);
			}
		}
	}
}

unsigned long mLast;
void processBits2(int v, unsigned long now, int N)
{ 
	if (v==0)
	{
		unsigned long d=now-mLast;
		mLast=now;
		printf("%1.1f ",(float)d/540.0);
	
		unsigned long timeSinceOrigin=now-(mOrigin);
		long cyclesSinceOrigin=(timeSinceOrigin+(mAvgLength/2))/mAvgLength;
		long closestOrigin=mOrigin+cyclesSinceOrigin*(long)mAvgLength;
		long timeSinceClosestOrigin=now-closestOrigin;
		
		mOrigin=closestOrigin;
		mAvgPhase256=((mAvgPhase256*(mIIRFactor-1))/mIIRFactor)+timeSinceClosestOrigin;
		int avgPhase=mAvgPhase256/mIIRFactor;
		mAvgPhase256-=avgPhase*mIIRFactor;
		
		long prox=timeSinceClosestOrigin>0?timeSinceClosestOrigin:-timeSinceClosestOrigin;
		/*
		if (prox>60)
		{	printf("B %d %d\n",timeSinceClosestOrigin,0);
			mBreak=1;
		}
		*/
		
		mAvgProx=((mAvgProx*(mIIRFactor-1))/mIIRFactor)+prox;
				
		// offset frequency to avoid phase offsetting
		if (mAvgLength>(mClock-mClockTolerance) && mAvgLength<(mClock+mClockTolerance))
		{
			mAvgLength256+=avgPhase/2;
			mAvgLength=(int)(mAvgLength256/mIIRFactor);
		}
		else
		{	mAvgLength256=mClock*mIIRFactor;
			mAvgLength=(int)(mAvgLength256/mIIRFactor);
			mAvgPhase256=0;
			//printf("reset\n");
		}

		n++;
		/*
		if (n==N)
		{	printf("d=%+03d avg_d=%02d period=%03d phase=%02d\n",timeSinceClosestOrigin, mAvgProx/mIIRFactor, mAvgLength, mAvgPhase  );
			n=0;
		}
		*/


	}
		
		
	
	/*
	if (mStarted)
	{
		//determine number of bits from origin
		float timeSinceOrigin=now-mOrigin;
		float bitsSinceOrigin=timeSinceOrigin/(float)mAvgLength;
		float bitsNow=mBitsAtOrigin+bitsSinceOrigin;
		
		//printf("%.2f -> %.2f (%d) \n",mLastBits,bitsNow, !v);
		// from mLastBits to bitsNow has the value !v
		
		//double lastBitFloor=floor((double)mLastBits);
		//double a=2.0;
		//double f=floor(a);
		
		//update current bit
		//float range=bitsNow>(int)
		
		
		mLastBits=bitsNow;
	}
	*/
		
}

float filterIIR(float aCurrent, float aNew, float aFactor, float aRange)
{	float result;
	float half=aRange*0.5;
	float factor2=1-aFactor;
	if (aCurrent>half && aNew<half)
	{	aNew+=aRange;
		result=aCurrent*factor2+aNew*aFactor;
		if (result>=aRange) result-=aRange;
	}
	else if (aCurrent<half && aNew>half)
	{	aNew-=aRange;
		result=aCurrent*factor2+aNew*aFactor;
		if (result<0) result+=aRange;
	}
	else
	{	result=aCurrent*factor2+aNew*aFactor;
	}
	return result;
}

float mCapPeriod=264.0;
unsigned long mCapBitCount=0;
float mCapBitPhase=0.0;
int mCapBitN;
void writeCaptureBits(int aValue, unsigned long aBitCount, float aOffset);
void capture(int v, unsigned long now)
{
	
	unsigned long per=(unsigned long)(mCapPeriod+0.49);
	unsigned long p=now-mOrigin;
	unsigned long c=p/per;
	unsigned long offset=c*per;
	mOrigin+=offset;
	mCapBitCount+=c;
	p-=offset;
	float r=((float)p)/mCapPeriod;
	
	if (v==0)
	{	r-=0.05; // lows are longer than highs
		if (r>=1.0)
		{	r-=1.0;
			mCapBitCount++;
		}
	}
	
	writeCaptureBits(!v,mCapBitCount,r);
	
	mCapBitPhase=filterIIR(mCapBitPhase,r,0.1,1.0);
	/*
	if (mCapBitPhase<0.2)
	{	mOrigin+=10;
	}
	*/
	if (mCapBitPhase>0.2 && mCapBitPhase<0.5)
	{	mOrigin+=80;
		//printf("+");
	}
	if (mCapBitPhase>0.5 && mCapBitPhase<0.8)
	{	mOrigin-=80;
		//printf("-");
	}
	/*
	if (mCapBitPhase>0.8)
	{	mOrigin-=10;
	}
	*/
	if (mCapBitN++>100)
	{	mCapBitN=0;
		//printf(" (%.2f) ",mCapBitPhase);
	}
	
	
}

unsigned long mWriteCaptureBitsLastCount;
float mWriteCaptureBitsLastOffset;
float mWriteCaptureBitsAccumulator;
void writeBit();
void writeCaptureBits(int aValue, unsigned long aBitCount, float aOffset)
{
	if (aBitCount!=	mWriteCaptureBitsLastCount)
	{
		if (aValue)
		{	float range=1-mWriteCaptureBitsLastOffset;
			mWriteCaptureBitsAccumulator+=range;
		}
		writeBit();
		
		int i;
		for (i=mWriteCaptureBitsLastCount+1; i<aBitCount; i++)
		{	mWriteCaptureBitsAccumulator=aValue?1.0:0.0;
			writeBit();
		}
		mWriteCaptureBitsLastOffset=0.0;
	}
	if (aValue)
	{	float range=aOffset-mWriteCaptureBitsLastOffset;
		mWriteCaptureBitsAccumulator+=range;
	}
	
	mWriteCaptureBitsLastCount=aBitCount;
	mWriteCaptureBitsLastOffset=aOffset;
}

char mWriteBitBuffer[7];
int mWriteBitIndex;
int mWriteBitGap;
int mWriteBitErrors;
void writeBit2(char aChar);
void writeBit()
{
	char bit=mWriteCaptureBitsAccumulator>=0.5?'1':'0';
	//printf("%s %.2f\n",mWriteCaptureBitsAccumulator>=0.5?"1":"0",mWriteCaptureBitsAccumulator);
	mWriteCaptureBitsAccumulator=0;
	
	if (mWriteBitIndex==0 && bit=='0')
	{	//sync on 1st bit high
		//if (mWriteBitGap%2==0) printf("*");
		mWriteBitGap++;
		return; 
	}
	if (mWriteBitGap==1)
	{	writeBit2('1');
		mWriteBitErrors++;
		//writeBit2('*');
	}
	else if (mWriteBitGap==2)
	{	writeBit2('1');
		mWriteBitErrors++;
	}
	else if (mWriteBitGap>20)
	{	writeBit2(0);
		mWriteBitErrors=0;
	}
	else if (mWriteBitGap>0)
	{	//printf("*(%d)",mWriteBitGap);
		for (; mWriteBitGap>0; mWriteBitGap-=2 )
		{	writeBit2('1'); //writeBit2('L');
			mWriteBitErrors++; 
			
		}
	}
	if (mWriteBitIndex==1 && bit=='1')
	{	//need 0
		writeBit2('1'); //writeBit2('%');
		mWriteBitIndex=0;
		mWriteBitErrors++; 
		return;
	}
	mWriteBitGap=0;
	mWriteBitBuffer[mWriteBitIndex++]=bit;
	if (mWriteBitIndex==6)
	{	
		if (strcmp(mWriteBitBuffer,"101010")==0)
		{	writeBit2('1'); writeBit2('1'); 
			memcpy(mWriteBitBuffer,"10",2);
			mWriteBitIndex=2;
		}
		else if (strcmp(mWriteBitBuffer,"100000")==0)
		{	writeBit2('1'); writeBit2('0');
			mWriteBitIndex=0;
		}
		else if (strcmp(mWriteBitBuffer,"101000")==0)
		{	writeBit2('1');
			memcpy(mWriteBitBuffer,"1000",4);
			mWriteBitIndex=4;
		}
		else if (strcmp(mWriteBitBuffer,"100001")==0)
		{	writeBit2('1'); writeBit2('1');
			memcpy(mWriteBitBuffer,"1",1);
			mWriteBitErrors++;
			mWriteBitIndex=1;
		}
		else if (strcmp(mWriteBitBuffer,"100010")==0)
		{	writeBit2('1'); writeBit2('1'); 
			memcpy(mWriteBitBuffer,"10",2);
			mWriteBitErrors++;
			mWriteBitIndex=2;
		}
		else
		{	writeBit2('1'); writeBit2('1');
			memcpy(mWriteBitBuffer,"10",2);
			mWriteBitErrors++;
			mWriteBitIndex=2; 
			//printf("(%s)",mWriteBitBuffer);
		}
	}
}

char filler[] = {'1','1','1','1','0','1','1','1','1','0','1','1','1','1','1','0','1','1' };
char mWriteBit2Buffer[100];
int  mWriteBit2Index;
int  mWriteBit2Errors=0;
void onMessage(char aChars[]);
void writeBit2(char aChar)
{	
	if (aChar==0)
	{
		if (mWriteBit2Index>80)
		{	
			if (mWriteBit2Errors)
			{
				mWriteBit2Buffer[mWriteBit2Index]=0;
				printf("ERROR %s \n",mWriteBit2Buffer);
				mWriteBit2Index=0;
				mWriteBit2Errors=0;
				return;
			}
			if (mWriteBit2Index<92)
  			{	//printf("expected 92 bits, got %d\n",mWriteBit2Index);
		  		int offset=92-mWriteBit2Index;
  				memmove(mWriteBit2Buffer+offset,mWriteBit2Buffer,mWriteBit2Index);
  				int i;
		  		int offsetRounded=1+(offset/9)*9;
		  		for (i=0; i<offset; i++)
		  		{	mWriteBit2Buffer[i]=filler[i];
		  		}
  				mWriteBit2Index=92;
  			}
  			else if (mWriteBit2Index>92)
  			{	int offset=mWriteBit2Index-92;
	  			memmove(mWriteBit2Buffer,mWriteBit2Buffer+offset,92);
	  			mWriteBit2Index=92;
  			}
  			
			mWriteBit2Buffer[mWriteBit2Index]=0;
			onMessage(mWriteBit2Buffer);
			//printf("%s %d \n",mWriteBit2Buffer, mWriteBit2Index);
			mWriteBit2Index=0;
			mWriteBit2Errors=0;
		}
	}
	else
	{	mWriteBit2Buffer[mWriteBit2Index++]=aChar;
		if (mWriteBit2Index>100)
		{	//printf("overflow\n");
			mWriteBit2Index=0;
			mWriteBit2Errors=0;
		}
		if (aChar!='0' && aChar!='1')
		{	mWriteBit2Errors=1;
		}
	}
}

byte nibbleLookup(byte a)
{
	switch (a)
	{	//case 0xff: return 0; //hack for start fix
		case 0xf6: return 0;
		case 0xee: return 1;
		case 0xed: return 2;
		case 0xeb: return 3;
		case 0xde: return 4;
		case 0xdd: return 5;
		case 0xdb: return 6;
		case 0xbe: return 7;
		case 0xbd: return 8;
		case 0xbb: return 9;
		case 0xb7: return 10;
		case 0x7e: return 11;
		case 0x7d: return 12;
		case 0x7b: return 13;
		case 0x77: return 14;
		case 0x6f: return 15;
	}
	//printf("bad nibble %#04x\n",a);
	return 16;	
}

byte onMessageNibbles[10];
void onMessage2(byte aNibbles[]);
void onMessage(char aChars[])
{
	int i=0;
	for (i=0; i<10; i++)
	{	int k=i*9;
		if (aChars[k]!='1')
  		{	//printf("framing bit of 1 not found before byte %d\n",(i+1));
  		}
  		byte d=0;
  		for (j=0; j<8; j++)
  		{	d=d<<1;
  			d|=(aChars[1+k+j]=='1')?1:0;
  		}
  		byte n=nibbleLookup(d);
  		if (n==16)
  		{	//printf("bad nibble %#04x on %d\n",d,i+1);
  			n=16;
  		}
  		onMessageNibbles[i]=n;
  	}
  	onMessage2(onMessageNibbles);
  	
	//printf("\nVALID %90s\n",aChars);
}

unsigned long mOnMessage2LastNow;
byte onMessage2Nibbles[10];
void addCandidate(int aId, int aSub, int aCmd, int aParam, int aErrors, unsigned long aTime);
void onMessage2(byte aNibbles[])
{
	unsigned long now = micros();
	int fixes=0;
	int i;
	if (now-mOnMessage2LastNow<1000000)
	{
	  	for (i=0; i<10; i++)
  		{	if (aNibbles[i]==16)
  			{	aNibbles[i]=onMessage2Nibbles[i];
	  			fixes++;
  			}
  			else
  			{	onMessage2Nibbles[i]=aNibbles[i];
  			}
  		}
  	}
  	if (fixes>1) return;
  	for (i=0; i<10; i++)
  	{	if (aNibbles[i]==16)
  		{	return;
  		}
  	}
	
	int parameter=(aNibbles[0]<<4)+aNibbles[1];
  	int subunit=aNibbles[2];
  	int command=aNibbles[3];
  	int id=0;
  	for (i=0; i<6; i++)
  	{	id+=(aNibbles[4+i]) << (4 * (5-i));
  	}
  	addCandidate(id,subunit,command,parameter,mWriteBitErrors,now);
  	//printf("id=%d sub=%d cmd=%d param=%d err=%d\n",id,subunit,command,parameter,mWriteBitErrors);
  	mOnMessage2LastNow=now;
}


void addCandidate(int aId, int aSub, int aCmd, int aParam, int aErrors, unsigned long aTime)
{
	if (aErrors<mACErrors || (aId!=mACId || aSub!=mACSub || aCmd!=mACCmd || aParam!=mACParam) )
	{	mACId=aId;
		mACSub=aSub;
		mACCmd=aCmd;
		mACParam=aParam;
		mACErrors=aErrors;
	}
	mACTime=aTime;
  	//printf("id=%d sub=%d cmd=%d param=%d err=%d\n",aId,aSub,aCmd,aParam,aErrors);
}


byte mLastValue=0;
void lw_process_bits()
{	
	byte v = digitalRead(mRxPin);
	if (v!=mLastValue)
	{	unsigned long now = micros();
		mLastValue=v;
		//processBits(v,now,100);
		capture(v,now);
	}
}


void lw_setup(int aPin)
{	mRxPin=aPin;
	wiringPiSetup();
	pinMode(mRxPin,INPUT);
	pullUpDnControl (mRxPin, PUD_DOWN);
	wiringPiISR(mRxPin,INT_EDGE_BOTH,lw_process_bits);
}

