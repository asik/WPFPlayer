// Taken from MSDN http://msdn.microsoft.com/en-us/library/sy1y3y1t.aspx
// Uses C++'s deterministic destruction to automatically free the lock

#pragma once

using namespace System::Threading;
ref class Lock {
   Object^ m_pObject;
public:
   Lock( Object ^ pObject ) : m_pObject( pObject ) {
      Monitor::Enter( m_pObject );
   }
   ~Lock() {
      Monitor::Exit( m_pObject );
   }
};