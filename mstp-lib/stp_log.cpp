
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2017 Adi Gostin, distributed under Apache License v2.0.

#define _CRT_SECURE_NO_WARNINGS

#include "stp_log.h"
#include "stp_bridge.h"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#ifdef _MSC_VER
	#define snprintf _snprintf
#endif

void STP_FlushLog (STP_BRIDGE* bridge)
{
	assert (bridge->logBufferUsedSize < bridge->logBufferMaxSize);

	bridge->logBuffer [bridge->logBufferUsedSize] = 0;
	bridge->callbacks.debugStrOut (bridge, bridge->logCurrentPort, bridge->logCurrentTree, bridge->logBuffer, bridge->logBufferUsedSize, true);
	bridge->logBufferUsedSize = 0;
}

static void WriteChar (STP_BRIDGE* bridge, int port, int tree, char c)
{
	// We're supposed to have enough space for the character and for a null-terminator,
	// in case we'll have to add it. Otherwise we have a bug somewhere in this function.
	assert (bridge->logBufferUsedSize <= bridge->logBufferMaxSize - 2);

	if (c == '\n')
	{
		// The line is completing.

		if (bridge->logLineStarting)
		{
			// Some library code called us with a line consisting of a single '\n' (without '\r').
			// We're not supposed to have something like this, as the library currently generates only "\r\n" line endings.
			assert (false);

			// When we'll support this, we should write logCurrentPort and logCurrentTree here, and maybe do something else too.
		}

		// We're supposed to have the same port and tree for a \n character.
		assert ((port == bridge->logCurrentPort) && (tree == bridge->logCurrentTree));

		bridge->logBuffer [bridge->logBufferUsedSize] = '\n';
		bridge->logBuffer [bridge->logBufferUsedSize + 1] = 0;
		bridge->callbacks.debugStrOut (bridge, port, tree, bridge->logBuffer, bridge->logBufferUsedSize + 1, false);
		bridge->logBufferUsedSize = 0;

		bridge->logLineStarting = true;
	}
	else
	{
		// Some other char.

		if (bridge->logLineStarting)
		{
			// First char on this line. We're supposed to have passed the previous line to the application (code above).
			assert (bridge->logBufferUsedSize == 0);

			// This new line might be for a different combination of port/tree, so let's update the values
			// before trying to indent, in case the buffer is really small and we need to call the callback
			// while writing the indentation spaces.
			bridge->logCurrentPort = port;
			bridge->logCurrentTree = tree;

			// This line _before_ writing any indentation chars.
			bridge->logLineStarting = false;

			for (unsigned int i = 0; i < bridge->logIndent; i++)
			{
				// Call ourselves recursively to keep things simple.
				WriteChar (bridge, port, tree, ' ');
			}
		}

		// We're somewhere in the middle of the line. We're not supposed to be changing
		// the tree or the port here, or else we have a bug somewhere in the library.
		assert ((port == bridge->logCurrentPort) && (tree == bridge->logCurrentTree));

		bridge->logBuffer [bridge->logBufferUsedSize] = c;
		bridge->logBufferUsedSize++;

		if (bridge->logBufferUsedSize == bridge->logBufferMaxSize - 1)
		{
			// We only have space for one additional char, so let's write the null-terminator and pass the buffer to the application.
			bridge->logBuffer [bridge->logBufferUsedSize] = 0;
			bridge->callbacks.debugStrOut (bridge, port, tree, bridge->logBuffer, bridge->logBufferUsedSize, false);
			bridge->logBufferUsedSize = 0;
		}
	}
}

void STP_Indent (STP_BRIDGE* bridge)
{
	// This is supposed to be called only at the start of the line.
	assert (bridge->logLineStarting);

	bridge->logIndent += STP_BRIDGE::LogIndentSize;
}

void STP_Unindent (STP_BRIDGE* bridge)
{
	// This is supposed to be called only at the start of the line.
	assert (bridge->logLineStarting);

	// We're not supposed to Unindent more than we Indent-ed.
	assert (bridge->logIndent >= STP_BRIDGE::LogIndentSize); // Trying to Unindent() more times than Indent()-ed.

	bridge->logIndent -= STP_BRIDGE::LogIndentSize;
}

void STP_Log (STP_BRIDGE* bridge, int port, int tree, const char* format, ...)
{
	char _buffer [10];

    va_list ap;
    va_start (ap, format);

	while (*format != 0)
	{
		if (strncmp (format, "{BID}", 5) == 0)
		{
			const BRIDGE_ID* bid = va_arg (ap, BRIDGE_ID*);
			STP_Log (bridge, port, tree, "{X4}.{BA}", bid->GetPriority (), &bid->GetAddress ());
			format += 5;
		}
		else if (strncmp (format, "{PID}", 5) == 0)
		{
			const PORT_ID* pid = va_arg (ap, PORT_ID*);
			if (pid->IsInitialized ())
			{
				unsigned short id = pid->GetPortIdentifier ();
				STP_Log (bridge, port, tree, "{X4}", (int) id);
			}
			else
				STP_Log (bridge, port, tree, "(undefined)");
			format += 5;
		}
		else if (strncmp (format, "{BA}", 4) == 0)
		{
			const unsigned char* a = va_arg (ap, unsigned char*);
			STP_Log (bridge, port, tree, "{X2}{X2}{X2}{X2}{X2}{X2}", a[0], a[1], a[2], a[3], a[4], a[5]);
			format += 4;
		}
		else if (strncmp (format, "{PVS}", 5) == 0)
		{
			const PRIORITY_VECTOR* pv = va_arg (ap, PRIORITY_VECTOR*);
			STP_Log (bridge, port, tree, "{BID}-{D7}-{BID}-{D7}-{BID}-{PID}",
					 &pv->RootId,
					 (int) pv->ExternalRootPathCost.GetValue (),
					 &pv->RegionalRootId,
					 (int) pv->InternalRootPathCost.GetValue (),
					 &pv->DesignatedBridgeId,
					 &pv->DesignatedPortId);
			format += 5;
		}
		else if (strncmp (format, "{S", 2) == 0)
		{
			format += 2;
			size_t size = 0;
			while (true)
			{
				char ch = *format;
				if ((ch < '0') || (ch > '9'))
					break;

				size = 10 * size + ch - '0';
				format++;
			}

			assert (*format == '}');
			format++;

			const char* str = va_arg (ap, const char*);
			size_t strLen = strlen (str);

			size_t paddingSize = (strLen >= size) ? 0 : (size - strLen);
			for (size_t i = 0; i < paddingSize; i++)
				WriteChar (bridge, port, tree, ' ');

			STP_Log (bridge, port, tree, str);
		}
		else if (strncmp (format, "{T}", 3) == 0)
		{
			unsigned int v = va_arg (ap, unsigned int);
			STP_Log (bridge, port, tree, "{D}.{D3}", (int) (v / 1000), (int) (v % 1000));
			format += 3;
		}
		/*
		else if (strncmp (format, "{I}", 3) == 0)
		{
			int i = va_arg (ap, int);
			while (i--)
				WriteChar (bridge, port, tree, ' ');

			format += 3;
		}
		*/
		else if (strncmp (format, "{TN}", 4) == 0)
		{
			int i = va_arg (ap, int);
			if (i == 0)
				STP_Log (bridge, port, tree, "CIST");
			else
				STP_Log (bridge, port, tree, "MST{D}", i);

			format += 4;
		}
		else if (strncmp (format, "{TMS}", 5) == 0)
		{
			const TIMES* times = va_arg (ap, TIMES*);
			STP_Log (bridge, port, tree, "MessageAge={D}, MaxAge={D}, HelloTime={D}, FwDelay={D}, remainingHops={D}",
					  times->MessageAge, times->MaxAge, times->HelloTime, times->ForwardDelay, times->remainingHops);
			format += 5;
		}
		else if (strncmp (format, "{D", 2) == 0)
		{
			format += 2;
			int size;
			if (*format >= '0' && *format <= '9')
			{
				size = *format - '0';
				format++;
			}
			else
				size = 0;

			assert (*format == '}');
			format++;

			int v = va_arg (ap, int);

			snprintf (_buffer, sizeof (_buffer), "%0*d", size, v);

			for (unsigned int i = 0; i < sizeof (_buffer) && _buffer [i] != 0; i++)
				WriteChar (bridge, port, tree, _buffer [i]);
		}
		else if (strncmp (format, "{X", 2) == 0)
		{
			format += 2;
			int size;
			if (*format >= '0' && *format <= '9')
			{
				size = *format - '0';
				format++;
			}
			else
				size = 0;

			assert (*format == '}');
			format++;

			int v = va_arg (ap, int);

			snprintf (_buffer, sizeof (_buffer), "%0*x", size, v);

			for (unsigned int i = 0; i < sizeof (_buffer) && _buffer [i] != 0; i++)
				WriteChar (bridge, port, tree, _buffer [i]);
		}
		else if (*format == '{')
		{
			assert (false); // not implemented
		}
		else
		{
			WriteChar (bridge, port, tree, *format);
			format++;
		}
	}

	va_end (ap);
}
