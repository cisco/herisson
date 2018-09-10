/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */
package com.cisco.vmi.testing;

import java.util.Vector;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.ExecutionException;

/**
 *
 * @author eshalev
 */
public abstract class BaseVmiTest extends BaseTest {

    protected FutureResult<Boolean> m_TestResult = new FutureResult();
    Thread m_VerificationThread = new Thread();

    public void setup() {
        System.out.println("Working Directory = " + System.getProperty("user.dir"));
        String root = "..\\..\\";
        /*addPath(root + "build\\windows\\x64\\Release;");
        addPath(root + "build\\vMIModules\\Release");
        addPath(root + "build\\libvMI\\Release");
        addPath(root + "external\\opencv\\build\\x64\\vc14\\bin");*/
    }

    Vector<PipelineComponnent> m_PipelineComponnents = new Vector<>();

    public final void addShellComponnent(ShellComponnent componnent) {
        componnent.m_ParentTest = this;
        _addComponnent(componnent);
    }

    public final void _addComponnent(PipelineComponnent componnent) {
        m_PipelineComponnents.add(componnent);
    }

    int m_NextComponnentId = 0;

    public String getNextPipelineComponnentId() {
        m_NextComponnentId++;
        return "" + m_NextComponnentId;
    }

    abstract public void verify() throws Exception;

    private void StartVerificationThread() {
        m_VerificationThread = new Thread(() -> {
            try {
                verify();
                m_TestResult.setIfFirst(Boolean.TRUE);
            }catch(Exception ex){
                m_TestResult.setExceptionIfFirst(ex);
            }
        });
        m_VerificationThread.start();
    }

    @Override
    public void run() {
        if (m_TestResult.triggered) {
            throw new RuntimeException("Test has allready been run");
        }
        if (m_PipelineComponnents.isEmpty()) {
            throw new UnsupportedOperationException("No tasks are queued");
        }
        for (PipelineComponnent c : m_PipelineComponnents) {
            c.configure(m_Config);
        }

        Vector<Thread> runningTasks = new Vector<>();

        try {
            for (PipelineComponnent c : m_PipelineComponnents) {
                System.out.println("Starting Task: " + c.getDescription());
                Thread t = new Thread(() -> {
                    try {
                        c.run();
                        //finish executing the test as soon as the first process is finished:
                        testCompleted();
                    } catch (Exception ex) {
                        ex.printStackTrace();
                        m_TestResult.setExceptionIfFirst(ex);
                    }
                });
                runningTasks.add(t);
                t.start();
                try {
                    //allow the pipeline componnent some time to start up before launching the next one
                    Thread.sleep(2000);
                } catch (InterruptedException ex) {
                    throw new RuntimeException(ex);
                }
            }

            //start verification thread:
            StartVerificationThread();
            try {
                //wait for first pipeline task to finish:
                m_TestResult.get();
            } catch (InterruptedException ex) {
                ex.printStackTrace();
                throw new RuntimeException(ex);
            } catch (ExecutionException ex) {
                ex.printStackTrace();
                throw new RuntimeException(ex);
            } //check that there is no error:

        } finally {
            System.out.println("Terminating all tasks");
            terminate();
            for (PipelineComponnent c : m_PipelineComponnents) {
                System.out.println("Stopping Thread: " + c.getDescription());
                c.terminate();
                c.await();
            }

        }

    }

    Vector<PipelineComponnent> m_RunningTasks = new Vector<>();

    public void terminate() {
        for (PipelineComponnent t : m_RunningTasks) {
            t.terminate();
        }
        if(m_VerificationThread!=null){
            m_VerificationThread.interrupt();
        }
    }
    
    public final void testCompleted(){
        m_TestResult.setIfFirst(Boolean.TRUE);
    }
    
    public final static void assertTrue(boolean condition,String message){
        if(!condition){
            throw new RuntimeException("Assertion Failed: " + message);
        }
    }

}
