/* 

xsample - extended sample objects for Max/MSP and pd (pure data)

Copyright (c) 2001,2002 Thomas Grill (xovo@gmx.net)
For information on usage and redistribution, and for a DISCLAIMER OF ALL
WARRANTIES, see the file, "license.txt," in this distribution.  

*/

#include "main.h"

#ifdef _MSC_VER
#pragma warning (disable:4244)
#endif


class xplay:
	public xsample
{
	FLEXT_HEADER_S(xplay,xsample,setup)

public:
	xplay(I argc, t_atom *argv);
	
#ifdef MAXMSP
	virtual V m_assist(L msg,L arg,C *s);
#endif

	virtual V m_help();
	virtual V m_print();
	
	virtual I m_set(I argc,t_atom *argv);

	virtual V m_start();
	virtual V m_stop();

protected:
	BL doplay;
	I outchns;

private:
	static V setup(t_class *c);

	virtual V m_dsp(I n,F *const *in,F *const *out);
	
	DEFSIGFUN(xplay)
	TMPLDEF V signal(I n,F *const *in,F *const *out);  // this is the dsp method
};

FLEXT_NEW_TILDE_G("xplay~",xplay)


V xplay::setup(t_class *)
{
#ifndef PD
	post("loaded xplay~ - part of xsample objects, version " XSAMPLE_VERSION " - (C) Thomas Grill, 2001-2002");
#endif
}


xplay::xplay(I argc, t_atom *argv): 
	doplay(false),
	outchns(1)
{
	I argi = 0;
#ifdef MAXMSP
	if(argc > argi && IsFlint(argv[argi])) {
		outchns = GetAFlint(argv[argi]);
		argi++;
	}
#endif

	if(argc > argi && IsSymbol(argv[argi])) {
		buf = new buffer(GetSymbol(argv[argi]),true);
		argi++;
		
#ifdef MAXMSP
		// oldstyle command line?
		if(argi == 1 && argc == 2 && IsFlint(argv[argi])) {
			outchns = GetAFlint(argv[argi]);
			argi++;
			post("%s: old style command line suspected - please change to '%s [channels] [buffer]'",thisName(),thisName()); 
		}
#endif
	}
	else
		buf = new buffer(NULL,true);

	AddInSignal();  // pos signal
	AddOutSignal(outchns);
	SetupInOut();
}


I xplay::m_set(I argc,t_atom *argv) 
{
	I r = xsample::m_set(argc,argv);
	if(r < 0) m_reset(); // resets pos/min/max
	if(r != 0) m_units();
	return r;
}

V xplay::m_start() 
{ 
	m_refresh(); 
	doplay = true; 
}

V xplay::m_stop() 
{ 
	doplay = false; 
}


TMPLDEF V xplay::signal(I n,F *const *invecs,F *const *outvecs)
{
	SIGCHNS(BCHNS,buf->Channels(),OCHNS,outchns);

	const F *pos = invecs[0];
	F *const *sig = outvecs;
	register I si = 0;

	if(doplay && buf->Frames() > 0) {
		if(interp == xsi_4p && buf->Frames() > 4) {
			I maxo = buf->Frames()-3;

			for(I i = 0; i < n; ++i,++si) {	
				F o = *(pos++)/s2u;
				I oint = (I)o;
				register F a,b,c,d;

				for(I ci = 0; ci < OCHNS; ++ci) {
					register const F *fp = buf->Data()+oint*BCHNS+ci;
					F frac = o-oint;

					if (oint < 1) {
						if(oint < 0) {
							frac = 0;
							a = b = c = d = buf->Data()[0*BCHNS+ci];
						}
						else {
							a = b = fp[0*BCHNS];
							c = fp[1*BCHNS];
							d = fp[2*BCHNS];
						}
					}
					else if(oint > maxo) {
						if(oint == maxo+1) {
							a = fp[-1*BCHNS];	
							b = fp[0*BCHNS];	
							c = d = fp[1*BCHNS];	
						}
						else {
							frac = 0; 
							a = b = c = d = buf->Data()[(buf->Frames()-1)*BCHNS+ci]; 
						}
					}
					else {
						a = fp[-1*BCHNS];
						b = fp[0*BCHNS];
						c = fp[1*BCHNS];
						d = fp[2*BCHNS];
					}

					F cmb = c-b;
					sig[ci][si] = b + frac*( 
						cmb - 0.5f*(frac-1.) * ((a-d+3.0f*cmb)*frac + (b-a-cmb))
					);
				}
			}
		}
		else {
			// no interpolation
			for(I i = 0; i < n; ++i,++si) {	
				I o = (I)(*(pos++)/s2u);
				if(o < 0) {
					for(I ci = 0; ci < OCHNS; ++ci)
						sig[ci][si] = buf->Data()[0*BCHNS+ci];
				}
				if(o > buf->Frames()) {
					for(I ci = 0; ci < OCHNS; ++ci)
						sig[ci][si] = buf->Data()[(buf->Frames()-1)*BCHNS+ci];
				}
				else {
					for(I ci = 0; ci < OCHNS; ++ci)
						sig[ci][si] = buf->Data()[o*BCHNS+ci];
				}
			}
		}	

		// clear rest of output channels (if buffer has less channels)
		for(I ci = OCHNS; ci < outchns; ++ci) 
			for(I i = 0; i < n; ++i) sig[ci][i] = 0;
	}
	else {
		for(I ci = 0; ci < outchns; ++ci) 
			for(I si = 0; si < n; ++si) sig[ci][si] = 0;
	}
}

V xplay::m_dsp(I /*n*/,F *const * /*insigs*/,F *const * /*outsigs*/)
{
	// this is hopefully called at change of sample rate ?!

	m_refresh();  

	switch(buf->Channels()*1000+outchns) {
		case 1001: 
			sigfun = SIGFUN(xplay,1,1); break;
		case 1002:
			sigfun = SIGFUN(xplay,1,2); break;
		case 2001:
			sigfun = SIGFUN(xplay,2,1); break;
		case 2002:
			sigfun = SIGFUN(xplay,2,2); break;
		case 4001:
		case 4002:
		case 4003:
			sigfun = SIGFUN(xplay,4,0); break;
		case 4004:
			sigfun = SIGFUN(xplay,4,4); break;
		default:
			sigfun = SIGFUN(xplay,0,0);
	}
}



V xplay::m_help()
{
	post("%s - part of xsample objects, version " XSAMPLE_VERSION,thisName());
#ifdef _DEBUG
	post("compiled on " __DATE__ " " __TIME__);
#endif
	post("(C) Thomas Grill, 2001-2002");
#ifdef MAXMSP
	post("Arguments: %s [channels=1] [buffer]",thisName());
#else
	post("Arguments: %s [buffer]",thisName());
#endif
	post("Inlets: 1:Messages/Position signal");
	post("Outlets: 1:Audio signal");	
	post("Methods:");
	post("\thelp: shows this help");
	post("\tset name: set buffer");
	post("\tenable 0/1: turn dsp calculation off/on");	
	post("\tprint: print current settings");
	post("\tbang/start: begin playing");
	post("\tstop: stop playing");
	post("\treset: checks buffer");
	post("\trefresh: checks buffer and refreshes outlets");
	post("\tunits 0/1/2/3: set units to samples/buffer size/ms/s");
	post("\tinterp 0/1: turn interpolation off/on");
	post("");
}

V xplay::m_print()
{
	// print all current settings
	post("%s - current settings:",thisName());
	post("bufname = '%s', length = %.3f, channels = %i",buf->Name(),(F)(buf->Frames()*s2u),buf->Channels()); 
	post("out channels = %i, samples/unit = %.3f, interpolation = %s",outchns,(F)(1./s2u),interp?"yes":"no"); 
	post("");
}


#ifdef MAXMSP
V xplay::m_assist(L msg,L arg,C *s)
{
	switch(msg) {
	case 1: //ASSIST_INLET:
		switch(arg) {
		case 0:
			sprintf(s,"Messages and Signal of playing position"); break;
		}
		break;
	case 2: //ASSIST_OUTLET:
		if(arg < outchns) 
			sprintf(s,"Audio signal channel %li",arg+1); 
		break;
	}
}
#endif



