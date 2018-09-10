/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */
package com.cisco.vmi.testing;

import java.util.concurrent.ExecutionException;

/**
 *
 * @author eshalev
 */
public class FutureResult<T> {

    T result = null;
    boolean triggered = false;
    Exception m_Exception;

    public synchronized T get(Long timeout) throws InterruptedException, ExecutionException {
        if (!triggered) {
            if (timeout != null) {
                wait(timeout);
            } else {
                wait();
            }
        }
        if (m_Exception != null) {
            throw new ExecutionException(m_Exception);
        }
        return result;
    }

    public synchronized T get() throws InterruptedException, ExecutionException {
        return get(null);
    }

    public synchronized void set(T result) {
        preventSettingTwice();
        this.result = result;
        this.triggered = true;
        notifyAll();
    }

    public synchronized void setIfFirst(T result) {
        if (!this.triggered) {
            set(result);
        }
    }
    
    public synchronized void setExceptionIfFirst(Exception e) {
        if (!this.triggered) {
            setException(e);
        }
    }

    private void preventSettingTwice() throws IndexOutOfBoundsException {
        if (triggered) {
            throw new IndexOutOfBoundsException("result has allready been triggered");
        }
    }

    public synchronized void setException(Exception e) {
        m_Exception = e;
        triggered = true;
        notifyAll();
    }

}
