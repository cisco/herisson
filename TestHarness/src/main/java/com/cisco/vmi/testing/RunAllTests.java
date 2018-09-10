/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */
package com.cisco.vmi.testing;

import com.cisco.vmi.testing.tests.test_lib_vmi_win.TestLibVmiWin;
import com.cisco.vmi.testing.tests.test_vmi_probe.TestVmiProbe;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.ExecutionException;

/**
 *
 * @author eshalev
 */
public class RunAllTests {

    /**
     * @param args the command line arguments
     */
    public static void main(String[] args) {
        System.out.println("Running All tests");
        Map<String, Exception> failedTests = new HashMap();
        Map<String, String> passedTests = new HashMap();
        ArrayList<BaseVmiTest> tests = new ArrayList<>();
        tests.add(new TestVmiProbe());
        tests.add(new TestLibVmiWin());
        for (final BaseVmiTest t : tests) {
            final BaseVmiTest test = t;
            String testName = t.toString();
            if (t.shouldRun()) {
                try {
                    runSingleTest(test);
                    passedTests.put(testName, "Passed");
                } catch (Exception ex) {
                    failedTests.put(testName, ex);
                }
            } else {
                passedTests.put(testName, "Skipped");
                System.err.println("Skipping test: " + t.toString());
            }

        }
        //print test results
        System.out.println(Helpers.Divider);
        System.out.println(Helpers.Divider);
        System.out.println(Helpers.Divider);
        System.out.println("Finished running tests");
        System.out.println("Passed tests:");
        for (String testName : passedTests.keySet()) {
            String testResult = passedTests.get(testName);
            System.out.println(testResult + " " + testName);
        }
        if (!failedTests.isEmpty()) {
            System.out.println(Helpers.Divider);
            System.out.println("Failed tests:");
            for (String testName : failedTests.keySet()) {
                System.out.println(Helpers.Divider);
                System.out.println("Failed! " + testName);
                Exception ex=failedTests.get(testName);
                System.out.println(Helpers.exceptionToString(ex));
            }
            throw new RuntimeException("Some tests have failed");
        } else {
            System.out.println(Helpers.Divider);
            System.out.println("All tests have passed");

        }

    }

    private static void runSingleTest(final BaseVmiTest test) {
        long start = System.currentTimeMillis();
        //new ShellExecutor(Arrays.asList(new String[]{"cmd","/C"," ping","www.google.com" }), new TestingConfiguration()).run();
        //System.exit(0);
        test.setup();

        Thread onShutDown = new Thread() {
            @Override
            public void run() {
                System.out.println("Shutdown hook ran!");
                test.terminate();
                System.out.println("Done shutting down");

            }
        };

        Runtime.getRuntime().addShutdownHook(onShutDown);
        try {
            System.out.println(Helpers.Divider);
            test.run();
        } finally {
            Runtime.getRuntime().removeShutdownHook(onShutDown);
            long duaration = System.currentTimeMillis() - start;
            System.out.println("duration: " + duaration);
        }

    }
    

}
