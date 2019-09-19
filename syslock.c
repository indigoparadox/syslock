
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <unistd.h>
#include <pwd.h>
#include <Xm/Xm.h>
#include <Xm/PushB.h>
#include <Xm/Label.h>
#include <Xm/RowColumn.h>

/* Prototype Callback function */

#define MSG_USAGE "usage: syslock -l [lockfile path] -p [wineprefix]\n"
#define ENV_SIZE 100
#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

static XtAppContext app = NULL;
static Widget wtop = NULL;

void pushed_fn( Widget, XtPointer, void* );

int dialog( char* msg ) {
	Widget wbutton = NULL,
		rc = NULL;

	rc = XtVaCreateManagedWidget( "dialog", xmRowColumnWidgetClass,
		wtop, XmNorientation, XmVERTICAL, NULL );

	XtVaCreateManagedWidget( msg, xmLabelWidgetClass, rc,
		NULL );

	wbutton = XtVaCreateManagedWidget( "button", xmPushButtonWidgetClass,
		rc, NULL );

	/* attach fn to widget */
	XtAddCallback( wbutton, XmNactivateCallback, pushed_fn, NULL );

	XtRealizeWidget( wtop ); /* display widget hierarchy */
	XtAppMainLoop( app ); /* enter processing loop */ 
}

void pushed_fn( Widget w, XtPointer client_data, void* cbsv ) {   
	//XmPushButtonCallbackStruct* cbs = (XmPushButtonCallbackStruct*)cbsv;
	exit( 1 );
}

int set_wine_env( char* penv[], int ienv, char* name, char* value ) {
	int ienvlen = 0,
		inamelen = 0,
		ivaluelen = 0;

	if( ENV_SIZE <= ienv ) {
		printf( "Environment size exceeded.\n" );
		return ienv;
	}

	if( NULL == name || NULL == value ) {
		printf( "Empty variable ignored.\n" );
		return ienv;
	}

	inamelen = strlen( name );
	ivaluelen = strlen( value );

	printf( "Got: %s (%d) %s (%d)\n", name, inamelen, value, ivaluelen );

	ienvlen = ivaluelen;
	/* if( (ienvlen * sizeof( int )) < ienvlen ) {
		printf( "Shrinking env buffer...\n" );
		ienvlen = (INT_MAX / sizeof( int )) -
			sizeof( int );
	} */
	ienvlen += 2; /* =, NULL */
	ienvlen += inamelen;

	penv[ienv] = calloc( ienvlen, sizeof( int ) );
	penv[ienv + 1] = NULL;

	snprintf(
		penv[ienv],
		ienvlen,
		"%s=%s",
		name, value
	);

	printf( "Set %d: %s\n", ienv, penv[ienv] );

	return ++ienv;
}

short arg_within_bounds( int argi, int argc ) {
	if( argi < argc ) {
		return TRUE;
	} else {
		dialog( MSG_USAGE );
		printf( MSG_USAGE );
		return FALSE;
	}
}

int main( int argc, char** argv ) {

	FILE* flock = NULL;
	//char cpid[11];
	char* plock = "/run/syslock/syslock.pid",
		* pwine = "/usr/bin/wine",
		* nuser = NULL;
	char* penv[ENV_SIZE] = { NULL };
	pid_t ipid = 0,
		ichildpid = 0;
	int istatus = 0,
		iargiter = 0,
		iwaitres = 0,
		ienv = 0;
	unsigned int ipid_tmp = 0;
	//int ipipe = 0;
	struct passwd* pw = NULL;

	XtSetLanguageProc( NULL, NULL, NULL );
	wtop = XtVaAppInitialize( &app, "Push", NULL, 0,
		&argc, argv, NULL, NULL );

	memset( penv, '\0', sizeof( char* ) * ENV_SIZE );

	ienv = set_wine_env( penv, ienv, "HOME", getenv( "HOME" ) );
	ienv = set_wine_env( penv, ienv, "DISPLAY", getenv( "DISPLAY" ) );
	ienv = set_wine_env( penv, ienv, "SHLVL", getenv( "SHLVL" ) );
	ienv = set_wine_env( penv, ienv, "PWD", getenv( "PWD" ) );

	ipid_tmp = getpid();
	printf( "My PID: %d\n", ipid_tmp );

	/* Process arguments. */
	while( iargiter + 1 < argc ) {
		iargiter++; /* Skip arg 0 */
		if( 0 == strncmp( argv[iargiter], "-l", 2 ) ) {
			/* -l : Lockfile Path */
			if( arg_within_bounds( iargiter + 1, argc ) ) {
				plock = argv[++iargiter];
				continue;
			}

		} else if( 0 == strncmp( argv[iargiter], "-p", 2 ) ) {
			/* -p : WINEPREFIX */
			if( arg_within_bounds( iargiter + 1, argc ) ) {
				iargiter++;
				ienv = set_wine_env(
					penv, ienv, "WINEPREFIX", argv[iargiter]
				);
				continue;
			}

		} else if( 0 == strncmp( argv[iargiter], "-w", 2 ) ) {
			/* -w : Wine Executable */
			if( arg_within_bounds( iargiter + 1, argc ) ) {
				pwine = argv[++iargiter];
				continue;
			}

		} else if( 0 == strncmp( argv[iargiter], "-u", 2 ) ) {
			/* -u : User */
			if( arg_within_bounds( iargiter + 1, argc ) ) {
				nuser = argv[++iargiter];
				continue;
			}
		}
	}

	/* Quit if lockfile exists. */
	flock = fopen( plock, "r" );
	if( NULL != flock ) {
		fscanf( flock, "%u", &ipid_tmp );
		ipid = ipid_tmp;
		fclose( flock );
		flock = NULL;
		if( getpgid( ipid ) >= 0 ) {
			dialog( "Unable to acquire lock for syslock. Is someone else using it?" );
		} else {
			printf( "Stale lockfile detected (%d). Improper shutdown last time?\n", ipid_tmp );
			unlink( plock );
			printf( "Lockfile removed.\n" );
		}
	}

	/* Create lockfile. */
	flock = fopen( plock, "w" );
	if( NULL == flock ) {
		dialog( "Unable to create lock file." );
	}
	fprintf( flock, "%ld", getpid() );
	fclose( flock );
	flock = NULL;

	/* Fork and execute. */
	ichildpid = fork();
	if( 0 == ichildpid ) {
		/* Open the comm pipe. */
		/*
		if( -1 == (ipipe = open( "/home/user/winelog.txt")) ) {
			dialog( "Unable to read child pipe." );
		}
		dup2( ipipe, STDOUT_FILENO );
		dup2( ipipe, STDERR_FILENO );
		close( ipipe );
		*/

		/* Set user. */
		if( NULL != nuser ) {
			pw = getpwnam( nuser );
		}
		if( NULL == pw ) {
			dialog( "Invalid user supplied." );
		} else {
			printf( "Starting as user: %d\n", pw->pw_uid );
			if( 0 != setgid( pw->pw_gid ) ) {
				dialog( "Unable to set primary group." );
			}
			if( 0 != setuid( pw->pw_uid ) ) {
				dialog( "Unable to set user." );
			}
		}

		ienv = 0;
		while( NULL != penv[ienv] ) {
			printf( "%s\n", penv[ienv] );
			ienv++;
		}

		printf( "Executing wine: %s\n", pwine );
		iwaitres = 
			execle( pwine, "wine", "c:/link/link.exe", (char*)NULL, penv );
		printf( "Exec fail: %d\n", errno );

	} else if( 0 < ichildpid ) {
		printf( "Waiting for syslock (%ld) to close...\n", ichildpid );
		iwaitres = wait( &istatus );
		if( 0 > iwaitres ) { 
			printf( "Error waiting for: %ld\n", ichildpid );
		}

	} else if( 0 > ichildpid ) {
		dialog( "Unable to launch syslock." );
	}

	unlink( plock );

	return 0;
}

