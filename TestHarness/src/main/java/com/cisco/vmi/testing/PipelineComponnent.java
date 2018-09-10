/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */
package com.cisco.vmi.testing;

import java.io.PrintWriter;
import java.io.StringWriter;
import java.util.concurrent.Callable;
import java.util.logging.Level;
import java.util.logging.Logger;

/**
 *
 * @author eshalev
 */
public interface PipelineComponnent extends Runnable{
    public String getDescription();
    public void configure(TestingConfiguration configuration);
    public void run();

    public void terminate();

    public void await();
}
