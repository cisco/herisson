/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */
package com.cisco.vmi.testing;

import java.io.File;
import java.io.IOException;
import java.io.PrintWriter;
import java.io.StringWriter;

/**
 *
 * @author eshalev
 */
public class Helpers {

    public static String exceptionToString(Exception ex) {
        StringWriter sw = new StringWriter();
        ex.printStackTrace(new PrintWriter(sw));
        String exceptionAsString = sw.toString();
        return exceptionAsString;
    }

    public static int waitForShellCommand(String command) throws IOException, InterruptedException{
        Process p = Runtime.getRuntime().exec(command);
        int exitVal = p.waitFor();
        return exitVal;
    }
    
    public static void assertThatFileExists(String videoFileName) throws RuntimeException {
        File f = new File(videoFileName);
        if (!f.exists() || f.isDirectory()) {
            throw new RuntimeException("Missing File: " + videoFileName);
        }
    }

    public static boolean isWindows() {
        String osName = System.getProperty("os.name");
        if (osName.toLowerCase().contains("linux")) {
            System.out.println("linux detected");
            return false;
        } else {
            System.out.println("windows detected");
            return true;
        }

    }

    public static String getExecutableExtensions() {
        return isWindows() ? ".exe" : "";
    }

    public static String removeColorCodesFromString(String stringWithLinuxColorCodes) {
        final String msgWithoutColorCodes
                = stringWithLinuxColorCodes.replaceAll("\u001B\\[[;\\d]*m", "");
        return msgWithoutColorCodes;
    }
    
    final public static String Divider="-------------------";
}
