/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */
package com.cisco.vmi.testing;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 *
 * @author eshalev
 */
public abstract class ShellComponnent implements PipelineComponnent{
    private List<String> m_ShellCommand=null;
    protected BaseVmiTest m_ParentTest;
    private ShellExecutor m_ShellExcecutor;
    public TestingConfiguration config;
    public String m_OutputPrefix="unnamed: ";
    //Subscribe for a line of stdout output
    public EventPublisher<String> m_OnStdOut=new EventPublisher<>();
    //Subscribe for a line of stderr output
    public EventPublisher<String> m_OnStdErr=new EventPublisher<>();
    //Subscribe for a line of either stdout output or stderr
    public EventPublisher<String> m_OnOutput=new EventPublisher<>();
   
            
    public void setShellCommand(List<String> shellCommand){
        m_ShellCommand=shellCommand;
    }
    
    public void setShellCommand(String shellCommand){
        m_ShellCommand=new ArrayList<>( );
        m_ShellCommand.addAll(Arrays.asList(shellCommand.split(" ")));
    }

    @Override
    public String getDescription() {
        if(m_ShellCommand==null){
            return "not set";
        }
        String ret="";
        for(String s : m_ShellCommand){
            ret+=s+" ";
        }
        return ret;
    }

    @Override
    public void run() {
        runShellCommand(m_ShellCommand,config);
    }
    
    void runShellCommand(List<String> toRun,TestingConfiguration config) {
        if(m_ShellExcecutor!=null){
            throw new RuntimeException("Task was allready launched: "+this.getDescription());
        }
        m_ShellExcecutor = new ShellExecutor(toRun,config);
        m_ParentTest.m_RunningTasks.add(this);
        //allow others to subscribe to outputs:
        m_ShellExcecutor.m_OnStdOut.pipeTo(m_OnStdOut);
        m_ShellExcecutor.m_OnStdErr.pipeTo(m_OnStdErr);
        m_ShellExcecutor.m_OnOutput.pipeTo(m_OnOutput);
        //print out messages by defaulr, with an identifier prepended to the task:
        m_OnStdErr.subscribe((msg)->System.err.println(m_OutputPrefix+" "+msg));
        m_OnStdOut.subscribe((msg)->System.out.println(m_OutputPrefix+" "+msg));
        m_ShellExcecutor.run();
    }
    
    
    @Override
    public void await(){
        if(m_ShellExcecutor!=null){
            m_ShellExcecutor.await();
        }
    }
    
    @Override
    public void terminate(){
        if(m_ShellExcecutor!=null){
            m_ShellExcecutor.terminate();
        }
    }
    
    public void configure(){
        //override this to intercept configuration changes
    }
    
    @Override
    final public void  configure(TestingConfiguration configuration){
        config=configuration;
        configure();
    }
}
