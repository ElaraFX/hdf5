/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Copyright by The HDF Group.                                               *
 * Copyright by the Board of Trustees of the University of Illinois.         *
 * All rights reserved.                                                      *
 *                                                                           *
 * This file is part of HDF5.  The full HDF5 copyright notice, including     *
 * terms governing use, modification, and redistribution, is contained in    *
 * the files COPYING and Copyright.html.  COPYING can be found at the root   *
 * of the source code distribution tree; Copyright.html can be found at the  *
 * root level of an installed copy of the electronic HDF5 document set and   *
 * is linked from the top-level documents page.  It can also be found at     *
 * http://hdfgroup.org/HDF5/doc/Copyright.html.  If you do not have          *
 * access to either file, you may request a copy from help@hdfgroup.org.     *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

package test;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import java.util.ArrayList;
import java.io.File;
import java.text.DecimalFormat;
import java.text.NumberFormat;
import java.nio.charset.StandardCharsets;

import hdf.hdf5lib.H5;
import hdf.hdf5lib.HDF5Constants;
import hdf.hdf5lib.HDFNativeData;
import hdf.hdf5lib.callbacks.H5P_cls_close_func_cb;
import hdf.hdf5lib.callbacks.H5P_cls_close_func_t;
import hdf.hdf5lib.callbacks.H5P_cls_copy_func_cb;
import hdf.hdf5lib.callbacks.H5P_cls_copy_func_t;
import hdf.hdf5lib.callbacks.H5P_cls_create_func_cb;
import hdf.hdf5lib.callbacks.H5P_cls_create_func_t;
import hdf.hdf5lib.callbacks.H5P_prp_set_func_cb;
import hdf.hdf5lib.callbacks.H5P_prp_get_func_cb;
import hdf.hdf5lib.callbacks.H5P_prp_delete_func_cb;
import hdf.hdf5lib.callbacks.H5P_prp_copy_func_cb;
import hdf.hdf5lib.callbacks.H5P_prp_compare_func_cb;
import hdf.hdf5lib.callbacks.H5P_prp_close_func_cb;
import hdf.hdf5lib.callbacks.H5P_prp_create_func_cb;
import hdf.hdf5lib.callbacks.H5P_iterate_cb;
import hdf.hdf5lib.callbacks.H5P_iterate_t;
import hdf.hdf5lib.exceptions.HDF5Exception;
import hdf.hdf5lib.exceptions.HDF5LibraryException;
import hdf.hdf5lib.structs.H5AC_cache_config_t;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestName;

public class TestH5Plist {
    @Rule public TestName testname = new TestName();

    // Property definitions
    private static final String CLASS1_NAME = "Class 1";
    private static final String CLASS1_PATH = "root/Class 1";

    private static final String CLASS2_NAME = "Class 2";
    private static final String CLASS2_PATH = "root/Class 1/Class 2";

    // Property definitions
    private static final String PROP1_NAME = "Property 1";
    private static final int    prop1_def = 10;   // Property 1 default value
    private static final int    PROP1_SIZE = 2;

    private static final String PROP2_NAME = "Property 2";
    private static final float  prop2_def = 3.14F;   // Property 2 default value
    private static final int    PROP2_SIZE = 8;

    private static final String PROP3_NAME  = "Property 3";
    private static final char[] prop3_def = {'T','e','n',' ','c','h','a','r','s',' '};   // Property 3 default value
    private static final int    PROP3_SIZE = 10;

    private static final String PROP4_NAME  = "Property 4";
    private static final double prop4_def = 1.41F;   // Property 4 default value
    private static final int    PROP4_SIZE = 8;

    private static final String [] pnames = { // Names of properties for iterator
            PROP1_NAME,
            PROP2_NAME,
            PROP3_NAME,
            PROP4_NAME};

    long plist_class_id = -1;

    @Before
    public void createPropClass()throws NullPointerException, HDF5Exception
    {
        assertTrue("H5 open ids is 0",H5.getOpenIDCount()==0);
        System.out.print(testname.getMethodName());
        // Create a new generic class, derived from the root of the class hierarchy
        try {
            plist_class_id = H5.H5Pcreate_class(HDF5Constants.H5P_ROOT, CLASS1_NAME, null, null, null, null, null, null);
        }
        catch (Throwable err) {
            err.printStackTrace();
            fail("TestH5Plist.H5Pcreate_class: " + err);
        }
        assertTrue(plist_class_id > 0);
    }

    @After
    public void deleteFileAccess() throws HDF5LibraryException {
        if (plist_class_id > 0)
            try {H5.H5Pclose(plist_class_id);} catch (Exception ex) {}
        System.out.println();
    }

    // Test basic generic property list code. Tests creating new generic classes.
    @Test
    public void testH5P_genprop_basic_class() {
        int         status = -1;
        long        cid1 = -1;        // Generic Property class ID
        long        cid2 = -1;        // Generic Property class ID
        long        cid3 = -1;        // Generic Property class ID
        String      name = null;       // Name of class

        try {
            // Check class name
            try {
                name = H5.H5Pget_class_name(plist_class_id);
            }
            catch (Throwable err) {
                err.printStackTrace();
                fail("H5Pget_class_name plist_class_id: " + err);
            }
            assertTrue("Class names don't match!, "+name+"="+CLASS1_NAME+"\n", name.compareTo(CLASS1_NAME)==0);

            // Check class parent
            try {
                cid2 = H5.H5Pget_class_parent(plist_class_id);
            }
            catch (Throwable err) {
                err.printStackTrace();
                fail("H5Pget_class_parent cid2: " + err);
            }

            // Verify class parent correct
            try {
                status = H5.H5Pequal(cid2, HDF5Constants.H5P_ROOT);
            }
            catch (Throwable err) {
                err.printStackTrace();
                fail("H5Pequal cid2: " + err);
            }
            assertTrue("H5Pequal cid2", status >= 0);

            // Make certain false postives aren't being returned
            try {
                status = H5.H5Pequal(cid2, HDF5Constants.H5P_FILE_CREATE);
            }
            catch (Throwable err) {
                err.printStackTrace();
                fail("H5Pequal cid2: " + err);
            }
            assertTrue("H5Pequal cid2", status >= 0);

            // Close parent class
            try {
                H5.H5Pclose_class(cid2);
                cid2 = -1;
            }
            catch (Throwable err) {
                err.printStackTrace();
                fail("H5Pclose_class cid2: " + err);
            }

            // Close class
            try {
                H5.H5Pclose_class(plist_class_id);
                plist_class_id = -1;
            }
            catch (Throwable err) {
                err.printStackTrace();
                fail("H5Pclose_class plist_class_id: " + err);
            }

            // Create another new generic class, derived from file creation class
            try {
                cid1 = H5.H5Pcreate_class(HDF5Constants.H5P_FILE_CREATE, CLASS2_NAME, null, null, null, null, null, null);
            }
            catch (Throwable err) {
                err.printStackTrace();
                fail("H5Pcreate_class cid1: " + err);
            }
            assertTrue("H5Pcreate_class cid1", cid1 >= 0);

            // Check class name
            try {
                name = H5.H5Pget_class_name(cid1);
            }
            catch (Throwable err) {
                err.printStackTrace();
                fail("H5Pget_class_name cid1: " + err);
            }
            assertTrue("Class names don't match!, "+name+"="+CLASS2_NAME+"\n", name.compareTo(CLASS2_NAME)==0);

            // Check class parent
            try {
                cid2 = H5.H5Pget_class_parent(cid1);
            }
            catch (Throwable err) {
                err.printStackTrace();
                fail("H5Pget_class_parent cid2: " + err);
            }
            assertTrue("H5Pget_class_parent cid2 ", cid2 >= 0);

            // Verify class parent correct
            try {
                status = H5.H5Pequal(cid2, HDF5Constants.H5P_FILE_CREATE);
            }
            catch (Throwable err) {
                err.printStackTrace();
                fail("H5Pequal cid2: " + err);
            }
            assertTrue("H5Pequal cid2 ", status >= 0);

            // Check class parent's parent
            try {
                cid3 = H5.H5Pget_class_parent(cid2);
            }
            catch (Throwable err) {
                err.printStackTrace();
                fail("H5Pget_class_parent cid3: " + err);
            }
            assertTrue("H5Pget_class_parent cid3", cid3 >= 0);

            // Verify class parent's parent correct
            try {
                status = H5.H5Pequal(cid3, HDF5Constants.H5P_GROUP_CREATE);
            }
            catch (Throwable err) {
                err.printStackTrace();
                fail("H5Pequal cid3: " + err);
            }
            assertTrue("H5Pequal cid3 ", status >= 0);

            // Close parent class's parent
            try {
                H5.H5Pclose_class(cid3);
                cid3 = -1;
            }
            catch (Throwable err) {
                err.printStackTrace();
                fail("H5Pclose_class cid3: " + err);
            }

            // Close parent class's parent
            try {
                H5.H5Pclose_class(cid2);
                cid2 = -1;
            }
            catch (Throwable err) {
                err.printStackTrace();
                fail("H5Pclose_class cid2: " + err);
            }

            // Close parent class's parent
            try {
                H5.H5Pclose_class(cid1);
                cid1 = -1;
            }
            catch (Throwable err) {
                err.printStackTrace();
                fail("H5Pclose_class cid1: " + err);
            }
        }
        finally {
            if (cid3 > 0)
                try {H5.H5Pclose_class(cid3);} catch (Throwable err) {}
            if (cid2 > 0)
                try {H5.H5Pclose_class(cid2);} catch (Throwable err) {}
            if (cid1 > 0)
                try {H5.H5Pclose_class(cid1);} catch (Throwable err) {}
        }
    }

    // Test basic generic property list code. Tests adding properties to generic classes.
    @Test
    public void testH5P_genprop_basic_class_prop() {
        int         status = -1;
        long        size = -1;        // Generic Property size
        long        nprops = -1;      // Generic Property class number

        // Check the number of properties in class
        try {
            nprops = H5.H5Pget_nprops(plist_class_id);
        }
        catch (Throwable err) {
            err.printStackTrace();
            fail("H5Pget_nprops plist_class_id: " + err);
        }
        assertTrue("H5Pget_nprops: "+nprops, nprops==0);

        // Check the existance of the first property (should fail)
        try {
            status = H5.H5Pexist(plist_class_id, PROP1_NAME);
        }
        catch (Throwable err) {
            err.printStackTrace();
            fail("H5Pexist plist_class_id: " + err);
        }
        assertTrue("H5Pexist plist_class_id "+PROP1_NAME, status == 0);

        // Insert first property into class (with no callbacks)
        try {
            byte[] prop_value = HDFNativeData.intToByte(prop1_def);

            H5.H5Pregister2(plist_class_id, PROP1_NAME, PROP1_SIZE, prop_value, null, null, null, null, null, null, null);
        }
        catch (Throwable err) {
            err.printStackTrace();
            fail("H5Pregister2 plist_class_id: "+PROP1_NAME + err);
        }

        // Try to insert the first property again (should fail)
        try {
            byte[] prop_value = HDFNativeData.intToByte(prop1_def);

            H5.H5Pregister2(plist_class_id, PROP1_NAME, PROP1_SIZE, prop_value, null, null, null, null, null, null, null);
            fail("H5Pregister2 plist_class_id: "+PROP1_NAME);
        }
        catch (Throwable err) {
        }

        // Check the existance of the first property
        try {
            status = H5.H5Pexist(plist_class_id, PROP1_NAME);
        }
        catch (Throwable err) {
            err.printStackTrace();
            fail("H5Pexist plist_class_id: " + err);
        }
        assertTrue("H5Pexist plist_class_id "+PROP1_NAME, status == 1);

        // Check the size of the first property
        try {
            size = H5.H5Pget_size(plist_class_id, PROP1_NAME);
        }
        catch (Throwable err) {
            err.printStackTrace();
            fail("H5Pget_size PROP1_NAME: " + err);
        }
        assertTrue("H5Pget_size "+PROP1_NAME +" size: "+size, size == PROP1_SIZE);

        // Check the number of properties in class
        try {
            nprops = H5.H5Pget_nprops(plist_class_id);
        }
        catch (Throwable err) {
            err.printStackTrace();
            fail("H5Pget_nprops plist_class_id: " + err);
        }
        assertTrue("H5Pget_nprops: "+nprops, nprops==1);

        // Insert second property into class (with no callbacks)
        try {
            byte[] prop_value = HDFNativeData.floatToByte(prop2_def);

            H5.H5Pregister2(plist_class_id, PROP2_NAME, PROP2_SIZE, prop_value, null, null, null, null, null, null, null);
        }
        catch (Throwable err) {
            err.printStackTrace();
            fail("H5Pregister2 plist_class_id: "+PROP2_NAME + err);
        }

        // Try to insert the second property again (should fail)
        try {
            byte[] prop_value = HDFNativeData.floatToByte(prop2_def);

            H5.H5Pregister2(plist_class_id, PROP2_NAME, PROP2_SIZE, prop_value, null, null, null, null, null, null, null);
            fail("H5Pregister2 plist_class_id: "+PROP2_NAME);
        }
        catch (Throwable err) {
        }

        // Check the existance of the second property
        try {
            status = H5.H5Pexist(plist_class_id, PROP2_NAME);
        }
        catch (Throwable err) {
            err.printStackTrace();
            fail("H5Pexist plist_class_id: " + err);
        }
        assertTrue("H5Pexist plist_class_id "+PROP2_NAME, status == 1);

        // Check the size of the second property
        try {
            size = H5.H5Pget_size(plist_class_id, PROP2_NAME);
        }
        catch (Throwable err) {
            err.printStackTrace();
            fail("H5Pget_size PROP2_NAME: " + err);
        }
        assertTrue("H5Pget_size "+PROP2_NAME +" size: "+size, size == PROP2_SIZE);

        // Check the number of properties in class
        try {
            nprops = H5.H5Pget_nprops(plist_class_id);
        }
        catch (Throwable err) {
            err.printStackTrace();
            fail("H5Pget_nprops plist_class_id: " + err);
        }
        assertTrue("H5Pget_nprops: "+nprops, nprops==2);

        // Insert third property into class (with no callbacks)
        try {
            byte[] prop_value = new String(prop3_def).getBytes(StandardCharsets.UTF_8);

            H5.H5Pregister2(plist_class_id, PROP3_NAME, PROP3_SIZE, prop_value, null, null, null, null, null, null, null);
        }
        catch (Throwable err) {
            err.printStackTrace();
            fail("H5Pregister2 plist_class_id: "+PROP3_NAME + err);
        }

        // Check the existance of the third property
        try {
            status = H5.H5Pexist(plist_class_id, PROP3_NAME);
        }
        catch (Throwable err) {
            err.printStackTrace();
            fail("H5Pexist plist_class_id: " + err);
        }
        assertTrue("H5Pexist plist_class_id "+PROP3_NAME, status == 1);

        // Check the size of the third property
        try {
            size = H5.H5Pget_size(plist_class_id, PROP3_NAME);
        }
        catch (Throwable err) {
            err.printStackTrace();
            fail("H5Pget_size PROP3_NAME: " + err);
        }
        assertTrue("H5Pget_size "+PROP3_NAME +" size: "+size, size == PROP3_SIZE);

        // Check the number of properties in class
        try {
            nprops = H5.H5Pget_nprops(plist_class_id);
        }
        catch (Throwable err) {
            err.printStackTrace();
            fail("H5Pget_nprops plist_class_id: " + err);
        }
        assertTrue("H5Pget_nprops: "+nprops, nprops==3);

        // Unregister first property
        try {
            H5.H5Punregister(plist_class_id, PROP1_NAME);
        }
        catch (Throwable err) {
            err.printStackTrace();
            fail("H5Punregister plist_class_id: "+PROP1_NAME + err);
        }

        // Try to check the size of the first property (should fail)
        try {
            size = H5.H5Pget_size(plist_class_id, PROP1_NAME);
            fail("H5Pget_size PROP1_NAME");
        }
        catch (Throwable err) {
        }

        // Check the number of properties in class
        try {
            nprops = H5.H5Pget_nprops(plist_class_id);
        }
        catch (Throwable err) {
            err.printStackTrace();
            fail("H5Pget_nprops plist_class_id: " + err);
        }
        assertTrue("H5Pget_nprops: "+nprops, nprops==2);

        // Unregister second property
        try {
            H5.H5Punregister(plist_class_id, PROP2_NAME);
        }
        catch (Throwable err) {
            err.printStackTrace();
            fail("H5Punregister plist_class_id: "+PROP2_NAME + err);
        }

        // Check the number of properties in class
        try {
            nprops = H5.H5Pget_nprops(plist_class_id);
        }
        catch (Throwable err) {
            err.printStackTrace();
            fail("H5Pget_nprops plist_class_id: " + err);
        }
        assertTrue("H5Pget_nprops: "+nprops, nprops==1);

        // Unregister third property
        try {
            H5.H5Punregister(plist_class_id, PROP3_NAME);
        }
        catch (Throwable err) {
            err.printStackTrace();
            fail("H5Punregister plist_class_id: "+PROP3_NAME + err);
        }

        // Check the number of properties in class
        try {
            nprops = H5.H5Pget_nprops(plist_class_id);
        }
        catch (Throwable err) {
            err.printStackTrace();
            fail("H5Pget_nprops plist_class_id: " + err);
        }
        assertTrue("H5Pget_nprops: "+nprops, nprops==0);
    }

    // Test basic generic property list code. Tests iterating over properties in a generic class.
    @Test
    public void testH5P_genprop_class_iter() {
        class idata {
            public String[] iter_names= null;
            public int iter_count = -1;
            idata(String[] names, int count) {
                this.iter_names = names;
                this.iter_count = count;
            }
        }
        class H5P_iter_data implements H5P_iterate_t {
            public ArrayList<idata> iterdata = new ArrayList<idata>();
        }
        H5P_iterate_t iter_data = new H5P_iter_data();

        class H5P_iter_callback implements H5P_iterate_cb {
            public int callback(long group, String name, H5P_iterate_t op_data) {
                idata id = ((H5P_iter_data)op_data).iterdata.get(0);
                return name.compareTo(id.iter_names[id.iter_count++]);
            }
        }
        H5P_iterate_cb iter_cb = new H5P_iter_callback();

        long        size = -1;        // Generic Property size
        long        nprops = -1;      // Generic Property class number
        int[]       idx = {0};        // Index to start iteration at

        // Insert first property into class (with no callbacks) */
        try {
            byte[] prop_value = HDFNativeData.intToByte(prop1_def);

            H5.H5Pregister2(plist_class_id, PROP1_NAME, PROP1_SIZE, prop_value, null, null, null, null, null, null, null);
        }
        catch (Throwable err) {
            err.printStackTrace();
            fail("H5Pregister2 plist_class_id: "+PROP1_NAME + err);
        }

        // Insert second property into class (with no callbacks) */
        try {
            byte[] prop_value = HDFNativeData.floatToByte(prop2_def);

            H5.H5Pregister2(plist_class_id, PROP2_NAME, PROP2_SIZE, prop_value, null, null, null, null, null, null, null);
        }
        catch (Throwable err) {
            err.printStackTrace();
            fail("H5Pregister2 plist_class_id: "+PROP2_NAME + err);
        }

        // Insert third property into class (with no callbacks) */
        try {
            byte[] prop_value = new String(prop3_def).getBytes(StandardCharsets.UTF_8);

            H5.H5Pregister2(plist_class_id, PROP3_NAME, PROP3_SIZE, prop_value, null, null, null, null, null, null, null);
        }
        catch (Throwable err) {
            err.printStackTrace();
            fail("H5Pregister2 plist_class_id: "+PROP3_NAME + err);
        }

        // Insert fourth property into class (with no callbacks) */
        try {
            byte[] prop_value = HDFNativeData.doubleToByte(prop4_def);

            H5.H5Pregister2(plist_class_id, PROP4_NAME, PROP4_SIZE, prop_value, null, null, null, null, null, null, null);
        }
        catch (Throwable err) {
            err.printStackTrace();
            fail("H5Pregister2 plist_class_id: "+PROP4_NAME + err);
        }

        // Check the number of properties in class */
        try {
            nprops = H5.H5Pget_nprops(plist_class_id);
        }
        catch (Throwable err) {
            err.printStackTrace();
            fail("H5Pget_nprops plist_class_id: " + err);
        }
        assertTrue("H5Pget_nprops: "+nprops, nprops==4);

        // Iterate over all properties in class */
        idata id = new idata(pnames, 0);
        ((H5P_iter_data)iter_data).iterdata.add(id);
        try {
            H5.H5Piterate(plist_class_id, null, iter_cb, iter_data);
        }
        catch (Throwable err) {
            err.printStackTrace();
            fail("H5.H5Piterate: " + err);
        }
        assertFalse("H5Piterate ",((H5P_iter_data)iter_data).iterdata.isEmpty());
        assertTrue("H5Piterate "+((H5P_iter_data)iter_data).iterdata.size(),((H5P_iter_data)iter_data).iterdata.size()==1);
        assertTrue("H5Piterate "+(((H5P_iter_data)iter_data).iterdata.get(0)).iter_count,((idata)((H5P_iter_data)iter_data).iterdata.get(0)).iter_count==4);

        // Iterate over last three properties in class */
        idx[0] = 1;
        ((H5P_iter_data)iter_data).iterdata.get(0).iter_count = 1;
        try {
            H5.H5Piterate(plist_class_id, idx, iter_cb, iter_data);
        }
        catch (Throwable err) {
            err.printStackTrace();
            fail("H5.H5Piterate: " + err);
        }
        assertFalse("H5Piterate ",((H5P_iter_data)iter_data).iterdata.isEmpty());
        assertTrue("H5Piterate "+((H5P_iter_data)iter_data).iterdata.size(),((H5P_iter_data)iter_data).iterdata.size()==1);
        assertTrue("H5Piterate "+(((H5P_iter_data)iter_data).iterdata.get(0)).iter_count,((idata)((H5P_iter_data)iter_data).iterdata.get(0)).iter_count==4);

        assertTrue("H5Piterate: "+nprops+"="+idx[0], nprops == idx[0]);
    }
}
