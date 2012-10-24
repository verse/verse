/**
 * $Id: user_auth.pml 709 2010-11-26 14:31:37Z jiri $
 *
 * This PROMELA program simulates and helps to verify the user
 * authentication used in Verse protocol over TCP/TLS connection.
 *
 * author: Jiri Hnidek <jiri.hnidek@tul.cz>
 */

#define UNVALID_USER		0
#define VALID_USER			1

#define UNVALID_DATA		0
#define VALID_DATA			1

#define AUTH_FAIL			0
#define AUTH_SUCC			1

#define AUTH_METH_RESV		0
#define AUTH_METH_NONE		1
#define AUTH_METH_DATA		2

#define MAX_AUTH_ATTEMPTS	3

#define NOT_EVIL			0
#define EVIL				1

#define UNSTABLE			0
#define STABLE				1

byte user, data;

/**
 * This process simulates the Verse client during user authentication. Every
 * process should be in the state named 'end*' at the end of the run.
 */
proctype Client(chan in; chan out)
{
	byte cmd_type;
	byte auth_meth;
	bit stable;
	
	/* This client could be stable or potentionaly unstable */
	do
	:: stable = STABLE -> { printf("Client: stable\n"); break; };
	:: stable = UNSTABLE -> { printf("Client: unstable\n"); break; };
	od;
		
	/* Random pick of valid and unvalid username and authentication data */
	do
		:: user = VALID_USER; data = VALID_DATA; break;
		:: user = VALID_USER; data = UNVALID_DATA; break;
		:: user = UNVALID_USER; data = UNVALID_DATA; break;
		:: user = UNVALID_USER; data = VALID_DATA; break;
	od;

/* First initial state */
closed:
	printf("Client: CLOSED\n");
	skip;

/* Second state; client sends username with authenticate method type NONE */
user_auth_none:
	printf("Client: USER_AUTH none\n");
	do
	:: out!user,AUTH_METH_NONE,0;
	:: in?cmd_type,auth_meth;
		if
		/* Server sent list of supported authenticated methods */
		:: (cmd_type==AUTH_FAIL &&
		    auth_meth==AUTH_METH_DATA) -> { goto user_auth_data; };
		/* Server should not close connection, when username is not valid */
		:: (cmd_type==AUTH_FAIL &&
		    auth_meth==AUTH_METH_RESV) -> { goto end_auth_fail; };
		/* Server should not authenticate user only due to username */
		:: (cmd_type==AUTH_SUCC) -> { goto end_auth_succ; };
		:: else -> { goto end_auth_fail; };
		fi;
	:: (stable==UNSTABLE) -> { goto end_auth_fail; };
	:: timeout -> goto end_auth_fail;
	od;

/* Third state; client sends username with authentication data */
user_auth_data:
	printf("Client: USER_AUTH data\n");
	do
	:: out!user,AUTH_METH_DATA,data;
	/* Try to receive some response from server */
	:: in?cmd_type,auth_meth;
		if
		/* Client was authenticated. */
		:: (cmd_type == AUTH_SUCC) -> { goto end_auth_succ; };
		/* Authentication of client failed. */
		:: (cmd_type == AUTH_FAIL &&
		    auth_meth == AUTH_METH_RESV) -> { goto end_auth_fail; };
		/* Authentication of client failed, but server gave the client one
		   more chance. */
		:: (cmd_type == AUTH_FAIL &&
		    auth_meth == AUTH_METH_DATA) -> { goto user_auth_data; };
		/* Other combinations are not allowed too. */
		:: else -> { goto end_auth_fail; };
		fi;
	:: (stable==UNSTABLE) -> { goto end_auth_fail; };
	:: timeout -> { goto end_auth_fail; };
	od;

/* Valid and unsuccessful end state */
end_auth_fail:
	printf("Client: FAILURE\n");
	
	/* Wait for server process */
	do
	:: in?cmd_type,auth_meth
	:: timeout -> { goto end; };
	od;

/* Valid and successful end state */
end_auth_succ:
	printf("Client: SUCCESS\n");

/* Valid end state */
end:
	skip;
}

/**
 * This process simulates the Verse server during user authentication.
 */
proctype Server(chan in; chan out)
{
	byte user_name, init_user_name;
	byte auth_meth;
	byte auth_data;
	byte attempts = 0;
	bit stable;
	
	/* The server could be stable or potentionaly unstable */
	do
	:: stable = STABLE -> { printf("Server: stable\n"); break; };
	:: stable = UNSTABLE -> { printf("Server: unstable\n"); break; };
	od;

/* First server state; server listen for client requests. */
listen:
	printf("Server: LISTEN\n");
	do
	:: in?user_name,auth_meth,auth_data;
		if
		/* First authentication attempt with method NONE -> send list of 
		   allowed methods. */
		:: (auth_meth == AUTH_METH_NONE) -> atomic {
				init_user_name = user_name;
				goto respond_user_auth;
			};
		/* Note: Real server should not authenticate user only
		   due to username. */
		:: (user_name == VALID_USER) -> { goto end_auth_succ; };
		/* Other combinations are not allowed. */
		:: else -> { goto end_auth_fail; }
		fi;
	:: (stable==UNSTABLE) -> goto end_auth_fail;
	:: timeout -> goto end_auth_fail;
	od;

/* Second server state; server sends list of supported methods and then listen
   for combination of username and authentication data. */
respond_user_auth:
	printf("Server: RESPOND user_auth\n");
	out!AUTH_FAIL,AUTH_METH_DATA;
	do
	:: in?user_name,auth_meth,auth_data;
		if
		:: (attempts < MAX_AUTH_ATTEMPTS &&
		    user_name == init_user_name &&
		    user_name == VALID_USER &&
		    auth_meth == AUTH_METH_DATA &&
		    auth_data == VALID_DATA) -> { goto end_auth_succ; };
		:: (attempts < MAX_AUTH_ATTEMPTS &&
		    user_name == init_user_name &&
		    user_name == VALID_USER &&
		    auth_meth == AUTH_METH_DATA &&
		    auth_data != VALID_DATA) -> atomic {
				attempts++;
				goto respond_user_auth;
		};
		:: (attempts < MAX_AUTH_ATTEMPTS &&
		    user_name == init_user_name &&
		    user_name != VALID_USER &&
		    auth_meth == AUTH_METH_DATA) -> atomic {
				attempts++;
				goto respond_user_auth;
		};
		:: (attempts >= MAX_AUTH_ATTEMPTS) -> { goto end_auth_fail; };
		:: (user_name != init_user_name) -> { goto end_auth_fail; };
		:: else -> { goto end_auth_fail; };
		fi;
	:: (stable==UNSTABLE) -> goto end_auth_fail;
	:: timeout -> goto end_auth_fail;
	od;

/* Valid and successful end state */
end_auth_succ:
	printf("Server: SUCCESS\n");
	out!AUTH_SUCC,0;
	goto end;

/* Valid and unsuccessful end state */
end_auth_fail:
	printf("Server: FAILED\n");
	do
	:: in?user_name,auth_meth,auth_data;
	:: timeout -> goto end;
	od;

/* Valid end state */
end:
	skip;
}

/**
 * Initial process, where communication channels are created and client and
 * server processes are started. Channels have zero length. It means that
 * channels are synchronous.
 */
init
{
	/* user_name, auth_type, auth_data */
	chan q1 = [0] of {bit, byte, bit};
	/* cmd_type, auth_meth */
	chan q2 = [0] of {bit, byte};
	
	atomic{run Server(q1, q2); run Client(q2, q1);};
}
