/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */
package com.cisco.vmi.testing;

import java.util.List;
import java.util.Vector;

/**
 *
 * @author eshalev
 */
public abstract class BaseTest implements Runnable {

    public TestingConfiguration m_Config = new TestingConfiguration();

    public void addPath(String path) {
        throw new UnsupportedOperationException("This method does not work");
        /*System.out.println("Adding path: "+path);
        m_Config.prependEnvironmentVariable("PATH", path+";");*/
    }

    abstract public void terminate();

    public boolean shouldRun() {
        return true;
    }

    @Override
    public String toString() {
        return this.getClass().getName();
    }

}
