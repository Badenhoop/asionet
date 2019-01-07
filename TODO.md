## TODO

### implementation
- google test
- repalcement for Busylock: stop(); waitUntilReady; runOperation()
    - for Timer (!!!), DatagramReceiver, ServiceServer
    
    - problem: recursive calls will block forever
    - need running flag
    - need either callstack marker or simply post next operation always
    
    operation(operation, handler, args)
    {
        wrappedHandler = { 
            handler();
            monitor {
                running = false;
                if (pendingOperation)
                    context.post(pendingOperation());
                pendingOperation.reset();
            }; 
        }   
        
        runningMonitor()
        {
            if (!running)
            {
                running = true;
                doOperation();
                return;
            }

            stop();
            pendingOperation = operation(args...);
    }
    
    Monitor<bool> runningMonitor;

### documentation
- doxygen 
- examples
- README

- CMAKE installation