// Conner Chew
// Words Without Friends: Web Server Edition
// words.c

#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
void *evaluateCommand(void *info);

char response[100000];    // 100000 bytes should hopefully be way more than enough to fit an entire game, no matter which word is selected.

struct thread_info    // Workaround since only one function attribute can be passed into a thread.
{
	char homepath[50];
	char command[100];
	int socket_id;
};

struct wordListNode
{
	char word[30];
	struct wordListNode *next;
};

struct gameListNode
{
	char word[30];
	bool hasBeenFound;
	struct gameListNode *next;
};

struct wordListNode *rootWord = NULL;
struct gameListNode *rootGame = NULL;
struct wordListNode *chosenWord = NULL;
int chosenWordDistribution[26];

int initialization();
struct wordListNode *getRandomWord(int wordCount);
void findWords(char masterWord[]);
void handleGame(char *url, char *buffer);
void displayWorld(char *buffer);
void acceptInput(char *url);
void getLetterDistribution(char word[], int wordDistribution[26]);
bool compareCounts(int wordDistribution[26], int ourDistribution[26]);
bool isDone();
void cleanupGameListNodes();
void cleanupWordListNodes();
void teardown();

int main(int argc, char **argv)
{
	if (argc != 2 || strncmp(argv[0], "./words\0", 8))    // Since the code relies on having the compiled program be named "words",
		printf("Usage: ./words <pwd>\n");                 // refuse to run if it is named anything else.
	else
	{
		struct addrinfo hints, *res;
		memset(&hints, 0, sizeof(hints));    // Ensures there is no garbage memory to prevent potential bugs.
		hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_flags = AI_PASSIVE;
		getaddrinfo("localhost", "8000", &hints, &res);
		
		int soc = socket(res->ai_family, res->ai_socktype, res->ai_protocol);    // This is just IPv4 and TCP, like above.
		bind(soc, res->ai_addr, res->ai_addrlen);
		listen(soc, 32);
		freeaddrinfo(res);
		
		int new_soc;
		struct sockaddr other_addr;
		socklen_t other_addr_size = sizeof(other_addr);
		char recvMsg[100] = {0};
		pthread_t threadID;
		struct thread_info attributes;
		memset(&attributes, 0, sizeof(attributes));
		strncpy(attributes.homepath, argv[1], 49);
		printf("Awaiting command...\n");
		while (1)
		{
			new_soc = accept(soc, &other_addr, &other_addr_size);
			recv(new_soc, recvMsg, 99, 0);
			if (strncmp(recvMsg, "QUIT", 4) != 0)    // I have provided a webclient program to make it possible to send a QUIT command.
			{
				attributes.socket_id = new_soc;
				strncpy(attributes.command, recvMsg, 100);
				pthread_create(&threadID, NULL, evaluateCommand, (void *)&attributes);    // Let the threads handle any command that isn't QUIT.
				pthread_join(threadID, NULL);
				close(new_soc);
			}
			else    // The purpose of this is to show that the program returns no errors or memory leaks in Valgrind (only if the program is QUIT as mentioned above).
			{
				printf("Shutting down...\n");    // This may not happen right away. Trying to access the words game again will allow this code to run (eventually).
				send(new_soc, "Shutting down server...", 24, 0);
				close(new_soc);
				break;
			}
		}
		
		close(soc);
		teardown();
	}
	return 0;
}

void *evaluateCommand(void *info)
{
	struct thread_info *attributes = (struct thread_info *)info;
	char *newlineRemover = attributes->command;
	while (*newlineRemover != '\n' && *newlineRemover != '\r')
		newlineRemover++;    // Traverse the command until the newline.
	*newlineRemover = '\0';
	printf("Command received: %s\n", attributes->command);
	if (strncmp(attributes->command, "GET ", 4) == 0)
	{
		char fullpath[150] = {0}, *httpChecker;
		strncpy(fullpath, attributes->homepath, 50);
		strncat(fullpath, attributes->command + 4, 100);    // The +4 takes out the GET part, leaving just the filepath provided.
		httpChecker = fullpath;
		while (strncmp(httpChecker, " HTTP/", 6) != 0)    // Remove the HTTP part from the filepath.
			httpChecker++;
		*httpChecker = '\0';
		while (strncmp(httpChecker, "/words", 6) != 0 && strncmp(httpChecker, "/favicon.ico", 12) != 0)
			httpChecker--;
		bool hasMove = false;
		httpChecker += 6;
		if (strncmp(httpChecker, "?move=", 6) == 0)
		{
			*httpChecker = '\0';    // Remove the question mark to open the file, then add it back later for the game.
			hasMove = true;
		}
		int fileID = open(fullpath, O_RDONLY);
		if (fileID < 0)
		{
			printf("File not found! Returning 404 response code.\n\n");
			send(attributes->socket_id, "HTTP/1.1 404 Not Found\r\nContent-Length:13\r\n\r\n404 Not Found", 65, 0);
		}
		else
		{
			if (hasMove)
				*httpChecker = '?';
			while (strncmp(httpChecker, "/words", 6) != 0)
				httpChecker--;
			printf("File found! Returning 200 response code.\n\n");
			memset(response, 0, 100000);
			strcpy(response, "HTTP/1.1 200 OK\r\ncontent-type: text/html; charset=UTF-8\r\n\r\n");
			handleGame(httpChecker, response);
			send(attributes->socket_id, response, 100000, 0);
			close(fileID);
		}
	}
	printf("Awaiting command...\n");
	return NULL;
}

int initialization()
{
	srand(time(0));
	FILE *wordFile = fopen("2of12.txt", "r");
	int wordCount = 0;
	char lastWord[30] = "\0", newWord[30] = "\0";    // Without these, the last word in the file (zymurgy) will duplicate due to there being a newline at the end.
	struct wordListNode *addWord = (struct wordListNode *) malloc(sizeof(struct wordListNode));
	rootWord = addWord;
	while (feof(wordFile) == 0)
	{
		fscanf(wordFile, "%s", newWord);    // newWord serves as a buffer for the linked list.
		if (strcmp(newWord, lastWord) != 0)
		{
			strcpy(addWord->word, newWord);
			strcpy(lastWord, addWord->word);    // lastWord enables the word in the previous node to be easily accessed.
			wordCount++;
			addWord->next = (struct wordListNode *) malloc(sizeof(struct wordListNode));
			addWord = addWord->next;
			memset(addWord, 0, sizeof(*addWord));
		}
	}
	fclose(wordFile);
	return wordCount;
}

struct wordListNode *getRandomWord(int wordCount)
{
	int chosenIndex = rand() % wordCount;
	struct wordListNode *currentWord = rootWord;
	for (int i = 0; i < chosenIndex; i++)
		currentWord = currentWord->next;
	while (currentWord != NULL && strlen(currentWord->word) <= 6)
		currentWord = currentWord->next;
	if (currentWord == NULL)
		currentWord = getRandomWord(wordCount);    // The function will call itself recursively if a sufficiently long word is not found.
	return currentWord;
}

void findWords(char masterWord[])
{
	getLetterDistribution(masterWord, chosenWordDistribution);
	struct wordListNode *currentWord = rootWord;
	struct gameListNode *currentGame = (struct gameListNode *) malloc(sizeof(struct gameListNode));
	rootGame = currentGame;
	int listWordDistribution[26] = {0};
	while (strlen(currentWord->word) != 0)    // The very last node in the end of the linked list has no word.
	{
		getLetterDistribution(currentWord->word, listWordDistribution);
		if (compareCounts(listWordDistribution, chosenWordDistribution))
		{
			strcpy(currentGame->word, currentWord->word);
			currentGame->hasBeenFound = false;
			currentGame->next = (struct gameListNode *) malloc(sizeof(struct gameListNode));
			currentGame = currentGame->next;
		}
		currentWord = currentWord->next;
		memset(listWordDistribution, 0, sizeof(listWordDistribution));    // Reset the distribution counter to be used for the next word.
	}
	struct gameListNode *tempAddress = currentGame;    // The purpose of these next few lines is to fix a segmentation fault that only occurred on some test runs of an early version of this program.
	free(currentGame);
	currentGame = rootGame;
	while (currentGame->next != tempAddress)
		currentGame = currentGame->next;
	currentGame->next = NULL;
}

void handleGame(char *url, char *buffer)
{
	acceptInput(url);
	displayWorld(buffer);
}

void displayWorld(char *buffer)
{
	strncat(buffer, "<html><head><style>p, input {font-family:Constantia;}</style></head><body><p>", 78);
	if (!isDone())    // If the game is not done, display the board.
	{
		char letterSpace[3] = {0};
		letterSpace[1] = ' ';
		for (int index = 0; index < 26; index++)
			for (int num = 0; num < chosenWordDistribution[index]; num++)
			{
				letterSpace[0] = index + 65;
				strncat(buffer, letterSpace, 3);    // Display the appropriate number of each (capitalized) letter in alphabetical order.
			}
		struct gameListNode *currentGame = rootGame;
		char *letterChecker;
		while (currentGame != NULL)
		{
			if (currentGame->hasBeenFound)
			{
				strncat(buffer, "<p>FOUND : ", 12);
				strncat(buffer, currentGame->word, 30);
				strncat(buffer, "\n", 2);
			}
			else
			{
				strncat(buffer, "<p>", 4);
				letterChecker = currentGame->word;    // The address of word is the same as the address of its first letter.
				while (*letterChecker != '\0')
				{
					strncat(buffer, "_ ", 3);
					letterChecker++;
				}
				strncat(buffer, "\n", 2);
			}
			currentGame = currentGame->next;
		}
		strncat(buffer, "<form submit=\"words\"><input type=\"text\" name=move maxlength=\"29\" autofocus></input></form>", 91);    // maxlength prevents the user from overflowing the HTTP request.
	}
	else    // Otherwise, display a congratulatory screen.
		strncat(buffer, "Congratulations! You solved it! <a href=\"words\">Another?</a>", 61);
	strncat(buffer, "</body></html>", 15);
}

void acceptInput(char *url)
{
	char *urlChecker = url + 1;    // Ignore the slash at the start of the GET request.
	if (strncmp(urlChecker, "words", 5) == 0)
	{
		urlChecker += 5;    // Jump to the ?move= part, if it is in the request.
		if (strncmp(urlChecker, "?move=", 6) == 0)
		{
			urlChecker += 6;    // Jump to the word the user typed in the input field.
			char guess[30];
			strncpy(guess, urlChecker, 30);
			struct gameListNode *currentGame = rootGame;
			while (currentGame != NULL)
			{
				if (strcmp(guess, currentGame->word) == 0)    // This is case sensitive, since there is I and i as well as O and o in the word list.
					currentGame->hasBeenFound = true;
				currentGame = currentGame->next;
			}
		}
		else
		{
			teardown();    // This will do nothing the first time since rootWord and rootGame are initialized to be NULL.
			memset(chosenWordDistribution, 0, sizeof(chosenWordDistribution));
			int wordCount = initialization();
			chosenWord = getRandomWord(wordCount);
			findWords(chosenWord->word);
		}
	}
}

void getLetterDistribution(char word[], int wordDistribution[26])
{
	int index = 0;
	while (word[index] != '\0')
	{
		if (word[index] >= 97)
			wordDistribution[word[index] - 97]++;    // Since 'a' is ASCII value 97, its counter is at index 0, and so on with the rest of the lowercase letters.
		else if (word[index] >= 65)
			wordDistribution[word[index] - 65]++;    // Likewise, 'A' is ASCII value 65 and so on with the rest of the uppercase letters.
		index++;
	}
}

bool compareCounts(int wordDistribution[26], int ourDistribution[26])
{
	for (int i = 0; i < 26; i++)
		if (wordDistribution[i] > ourDistribution[i])
			return false;    // If even one letter goes over count, the word will fail the comparison test.
	return true;    // This block will only be reached if every letter passes the test.
}

bool isDone()
{
	struct gameListNode *currentGame = rootGame;
	while (currentGame != NULL)
	{
		if (!(currentGame->hasBeenFound))
			return false;
		currentGame = currentGame->next;
	}
	return true;
}

void cleanupGameListNodes()
{
	struct gameListNode *currentGame = rootGame;
	while (currentGame != NULL)
	{
		currentGame = currentGame->next;
		free(rootGame);
		rootGame = currentGame;
	}
}

void cleanupWordListNodes()
{
	struct wordListNode *currentWord = rootWord;
	while (currentWord != NULL)
	{
		currentWord = currentWord->next;
		free(rootWord);
		rootWord = currentWord;
	}
}

void teardown()
{
	cleanupGameListNodes();
	cleanupWordListNodes();
}
