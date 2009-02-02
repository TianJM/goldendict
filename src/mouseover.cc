#include "mouseover.hh"
#include "utf8.hh"
#include <QCoreApplication>
#include <QDir>

#ifdef Q_OS_WIN32
#include "mouseover_win32/ThTypes.h"
#endif

MouseOver & MouseOver::instance()
{
  static MouseOver m;

  return m;
}

#ifdef Q_OS_WIN32
const UINT WM_MY_SHOW_TRANSLATION = WM_USER + 301;
static wchar_t className[] = L"GoldenDictMouseover";
#endif

MouseOver::MouseOver()
{
#ifdef Q_OS_WIN32

  ThTypes_Init();
  memset( GlobalData, 0, sizeof( TGlobalDLLData ) );
  strcpy( GlobalData->LibName,
    QDir::toNativeSeparators( QDir( QCoreApplication::applicationDirPath() ).filePath( "GdTextOutHook.dll" ) ).toLocal8Bit().data() );

  // Create the window to recive spying results to

  WNDCLASSEX wcex;

  wcex.cbSize = sizeof( WNDCLASSEX );

  wcex.style	        = 0;
  wcex.lpfnWndProc	= ( WNDPROC ) eventHandler;
  wcex.cbClsExtra		= 0;
  wcex.cbWndExtra		= 0;
  wcex.hInstance		= GetModuleHandle( 0 );
  wcex.hIcon		= NULL;
  wcex.hCursor		= NULL,
  wcex.hbrBackground	= NULL;
  wcex.lpszMenuName	= NULL;
  wcex.lpszClassName	= className;
  wcex.hIconSm		= NULL;

  RegisterClassEx( &wcex );

  GlobalData->ServerWND = CreateWindow( className, L"", 0, 0, 0, 0, 0, GetDesktopWindow(), NULL, GetModuleHandle( 0 ), 0 );

  spyDll = LoadLibrary( QDir::toNativeSeparators( QDir( QCoreApplication::applicationDirPath() ).filePath( "GdTextOutSpy.dll" ) ).toStdWString().c_str() );

  if ( spyDll )
  {
    activateSpyFn = ( ActivateSpyFn ) GetProcAddress( spyDll, "ActivateTextOutSpying" );

    if ( activateSpyFn )
      activateSpyFn( true );
  }

#endif
}

#ifdef Q_OS_WIN32

LRESULT CALLBACK MouseOver::eventHandler( HWND hwnd, UINT msg,
                                          WPARAM wparam, LPARAM lparam )
{
  if ( msg == WM_MY_SHOW_TRANSLATION )
  {
    int wordSeqPos;
    QString wordSeq;

    // Is the string in utf8 or in locale encoding?

    wchar_t testBuf[ 256 ];

    long result = Utf8::decode( GlobalData->CurMod.MatchedWord,
                                strlen( GlobalData->CurMod.MatchedWord ),
                                testBuf );

    if ( result >= 0 )
    {
      // It seems to be
      QString begin = QString::fromUtf8( GlobalData->CurMod.MatchedWord,
                                         GlobalData->CurMod.BeginPos ).normalized( QString::NormalizationForm_C );

      QString end = QString::fromUtf8( GlobalData->CurMod.MatchedWord +
                                       GlobalData->CurMod.BeginPos ).normalized( QString::NormalizationForm_C );

      wordSeq = begin + end;
      wordSeqPos = begin.size();
    }
    else
    {
      // It's not, so interpret it as in local encoding
      QString begin = QString::fromLocal8Bit( GlobalData->CurMod.MatchedWord,
                                         GlobalData->CurMod.BeginPos ).normalized( QString::NormalizationForm_C );

      QString end = QString::fromLocal8Bit( GlobalData->CurMod.MatchedWord +
                                       GlobalData->CurMod.BeginPos ).normalized( QString::NormalizationForm_C );

      wordSeq = begin + end;
      wordSeqPos = begin.size();
    }

    // Now locate the word inside the sequence

    QString word;

    if ( wordSeq[ wordSeqPos ].isSpace() )
    {
      // Currently we ignore such cases
      return DefWindowProc( hwnd, msg, wparam, lparam );
    }
    else
    if ( !wordSeq[ wordSeqPos ].isLetterOrNumber() )
    {
      // Special case: the cursor points to something which doesn't look like a
      // middle of the word -- assume that it's something that joins two words
      // together.

      int begin = wordSeqPos;

      for( ; begin; --begin )
        if ( !wordSeq[ begin - 1 ].isLetterOrNumber() )
          break;

      int end = wordSeqPos;

      while( ++end < wordSeq.size() )
        if ( !wordSeq[ end ].isLetterOrNumber() )
          break;

      if ( end - begin == 1 )
      {
        // Well, turns out it was just a single non-letter char, discard it
        return DefWindowProc( hwnd, msg, wparam, lparam );
      }

      word = wordSeq.mid( begin, end - begin );
    }
    else
    {
      // Cursor points to a letter -- cut the word it points to

      int begin = wordSeqPos;

      for( ; begin; --begin )
        if ( !wordSeq[ begin - 1 ].isLetterOrNumber() )
          break;

      int end = wordSeqPos;

      while( ++end < wordSeq.size() )
      {
        if ( !wordSeq[ end ].isLetterOrNumber() )
          break;
      }
      word = wordSeq.mid( begin, end - begin );
    }

    emit instance().hovered( word );
  }

  return DefWindowProc( hwnd, msg, wparam, lparam );
}

#endif

MouseOver::~MouseOver()
{
#ifdef Q_OS_WIN32

  if ( activateSpyFn )
    activateSpyFn( false );

  FreeLibrary( spyDll );

  DestroyWindow( GlobalData->ServerWND );

  UnregisterClass( className, GetModuleHandle( 0 ) );

  Thtypes_End();

#endif
}

