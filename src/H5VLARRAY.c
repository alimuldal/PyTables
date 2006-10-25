#include "H5ATTR.h"
#include "tables.h"
#include "utils.h"
#include "H5Zlzo.h"  		       /* Import FILTER_LZO */
#include "H5Zbzip2.h"  		       /* Import FILTER_BZIP2 */
#include <string.h>
#include <stdlib.h>


/*-------------------------------------------------------------------------
 *
 * Public functions
 *
 *-------------------------------------------------------------------------
 */


/*-------------------------------------------------------------------------
 * Function: H5VLARRAYmake
 *
 * Purpose: Creates and writes a dataset of a variable length type type_id
 *
 * Return: Success: 0, Failure: -1
 *
 * Programmer: F. Altet
 *
 * Date: November 08, 2003
 *-------------------------------------------------------------------------
 */

herr_t H5VLARRAYmake( hid_t loc_id,
		      const char *dset_name,
		      const char *class_,
		      const char *title,
		      const char *flavor,
		      const char *obversion,    /* The Array VERSION number */
		      const int rank,
		      const int scalar,
		      const hsize_t *dims,
		      hid_t type_id,
		      hsize_t chunk_size,
		      void  *fill_data,
		      int   compress,
		      char  *complib,
		      int   shuffle,
		      int   fletcher32,
		      const void *data)
{

 hvl_t   vldata;
 hid_t   dataset_id, space_id, datatype, tid1;
 hsize_t dataset_dims[1];
 hsize_t maxdims[1] = { H5S_UNLIMITED };
 hsize_t dims_chunk[1];
 hid_t   plist_id;
 unsigned int cd_values[2];

 if (data)
   /* if data, one row will be filled initially */
   dataset_dims[0] = 1;
 else
   /* no data, so no rows on dataset initally */
   dataset_dims[0] = 0;

 dims_chunk[0] = chunk_size;

 /* Fill the vldata estructure with the data to write */
 /* This is currectly not used */
 vldata.p = (void *)data;
 vldata.len = 1;		/* Only one array type to save */

 /* Create a VL datatype */
 if (scalar == 1) {
   datatype = H5Tvlen_create(type_id);
 }
 else {
   tid1 = H5Tarray_create(type_id, rank, dims, NULL);
   datatype = H5Tvlen_create(tid1);
   H5Tclose( tid1 );   /* Release resources */
 }

 /* The dataspace */
 space_id = H5Screate_simple( 1, dataset_dims, maxdims );

 /* Modify dataset creation properties, i.e. enable chunking  */
 plist_id = H5Pcreate (H5P_DATASET_CREATE);
 if ( H5Pset_chunk ( plist_id, 1, dims_chunk ) < 0 )
   return -1;

 /*
    Dataset creation property list is modified to use
 */

 /* Fletcher must be first */
 if (fletcher32) {
   if ( H5Pset_fletcher32( plist_id) < 0 )
     return -1;
 }
 /* Then shuffle */
 if (shuffle) {
   if ( H5Pset_shuffle( plist_id) < 0 )
     return -1;
 }
 /* Finally compression */
 if (compress) {
   cd_values[0] = compress;
   cd_values[1] = (int)(atof(obversion) * 10);
   cd_values[2] = VLArray;
   /* The default compressor in HDF5 (zlib) */
   if (strcmp(complib, "zlib") == 0) {
     if ( H5Pset_deflate( plist_id, compress) < 0 )
       return -1;
   }
   /* The LZO compressor does accept parameters */
   else if (strcmp(complib, "lzo") == 0) {
     if ( H5Pset_filter( plist_id, FILTER_LZO, H5Z_FLAG_OPTIONAL, 3, cd_values) < 0 )
       return -1;
   }
   /* The bzip2 compress does accept parameters */
   else if (strcmp(complib, "bzip2") == 0) {
     if ( H5Pset_filter( plist_id, FILTER_BZIP2, H5Z_FLAG_OPTIONAL, 3, cd_values) < 0 )
       return -1;
   }
   else {
     /* Compression library not supported */
     fprintf(stderr, "Compression library not supported\n");
     return -1;
   }
 }

 /* Create the dataset. */
 if ((dataset_id = H5Dcreate(loc_id, dset_name, datatype, space_id, plist_id )) < 0 )
   goto out;

 /* Write the dataset only if there is data to write */
 if (data)
   if ( H5Dwrite( dataset_id, datatype, H5S_ALL, H5S_ALL, H5P_DEFAULT, &vldata ) < 0 )
     goto out;

 /* Terminate access to the data space. */
 if ( H5Sclose( space_id ) < 0 )
  return -1;

 /* Release the datatype in the case that it is not an atomic type */
 if ( H5Tclose( datatype ) < 0 )
   return -1;

 /* End access to the property list */
 if ( H5Pclose( plist_id ) < 0 )
   goto out;

/*-------------------------------------------------------------------------
 * Set the conforming array attributes
 *-------------------------------------------------------------------------
 */

 /* Attach the CLASS attribute */
 if ( H5ATTRset_attribute_string( dataset_id, "CLASS", class_ ) < 0 )
  goto out;

 /* Attach the CLASS attribute */
 if ( H5ATTRset_attribute_string( dataset_id, "FLAVOR", flavor ) < 0 )
  goto out;

 /* Attach the VERSION attribute */
 if ( H5ATTRset_attribute_string( dataset_id, "VERSION", obversion ) < 0 )
  goto out;

 /* Attach the TITLE attribute */
 if ( H5ATTRset_attribute_string( dataset_id, "TITLE", title ) < 0 )
  goto out;

 return dataset_id;

out:

 return -1;

}

/*-------------------------------------------------------------------------
 * Function: H5ARRAYappend_records
 *
 * Purpose: Appends records to an array
 *
 * Return: Success: 0, Failure: -1
 *
 * Programmers:
 *  Francesc Altet
 *
 * Date: October 30, 2003
 *
 * Comments: Uses memory offsets
 *
 * Modifications:
 *
 *
 *-------------------------------------------------------------------------
 */


herr_t H5VLARRAYappend_records( hid_t dataset_id,
				hid_t type_id,
				int nobjects,
				hsize_t nrecords,
				const void *data )
{

 hid_t    space_id;
 hid_t    mem_space_id;
 hsize_t  start[1];
 hsize_t  dataset_dims[1];
 hsize_t  dims_new[1] = {1};	/* Only a record on each append */
 hvl_t    wdata;   /* Information to write */


 /* Initialize VL data to write */
 wdata.p=(void *)data;
 wdata.len=nobjects;

 /* Dimension for the new dataset */
 dataset_dims[0] = nrecords + 1;

 /* Extend the dataset */
 if ( H5Dextend ( dataset_id, dataset_dims ) < 0 )
  goto out;

 /* Create a simple memory data space */
 if ( (mem_space_id = H5Screate_simple( 1, dims_new, NULL )) < 0 )
  return -1;

 /* Get the file data space */
 if ( (space_id = H5Dget_space( dataset_id )) < 0 )
  return -1;

 /* Define a hyperslab in the dataset */
 start[0] = nrecords;
 if ( H5Sselect_hyperslab( space_id, H5S_SELECT_SET, start, NULL, dims_new, NULL) < 0 )
   goto out;

 if ( H5Dwrite( dataset_id, type_id, mem_space_id, space_id, H5P_DEFAULT, &wdata ) < 0 )
     goto out;

 /* Terminate access to the dataspace */
 if ( H5Sclose( space_id ) < 0 )
  goto out;

 if ( H5Sclose( mem_space_id ) < 0 )
  goto out;

return 1;

out:
 return -1;

}


/*-------------------------------------------------------------------------
 * Function: H5ARRAYmodify_records
 *
 * Purpose: Modify records of an array
 *
 * Return: Success: 0, Failure: -1
 *
 * Programmers:
 *  Francesc Altet
 *
 * Date: October 28, 2004
 *
 * Comments: Uses memory offsets
 *
 * Modifications:
 *
 *
 *-------------------------------------------------------------------------
 */

herr_t H5VLARRAYmodify_records( hid_t dataset_id,
				hid_t type_id,
				hsize_t nrow,
				int nobjects,
				const void *data )
{

 hid_t    space_id;
 hid_t    mem_space_id;
 hsize_t  start[1];
 hsize_t  dims_new[1] = {1};	/* Only a record on each update */
 hvl_t    wdata;   /* Information to write */

 /* Initialize VL data to write */
 wdata.p=(void *)data;
 wdata.len=nobjects;

 /* Create a simple memory data space */
 if ( (mem_space_id = H5Screate_simple( 1, dims_new, NULL )) < 0 )
  return -1;

 /* Get the file data space */
 if ( (space_id = H5Dget_space( dataset_id )) < 0 )
  return -1;

 /* Define a hyperslab in the dataset */
 start[0] = nrow;
 if ( H5Sselect_hyperslab( space_id, H5S_SELECT_SET, start, NULL, dims_new, NULL) < 0 )
   goto out;

 if ( H5Dwrite( dataset_id, type_id, mem_space_id, space_id, H5P_DEFAULT, &wdata ) < 0 )
     goto out;

 /* Terminate access to the dataspace */
 if ( H5Sclose( space_id ) < 0 )
  goto out;

 if ( H5Sclose( mem_space_id ) < 0 )
  goto out;

return 1;

out:
 return -1;

}


/*-------------------------------------------------------------------------
 * Function: H5VLARRAYget_ndims
 *
 * Purpose: Gets the dimensionality of an array.
 *
 * Return: Success: 0, Failure: -1
 *
 * Programmer: Francesc Altet
 *
 * Date: November 19, 2003
 *
 *-------------------------------------------------------------------------
 */

herr_t H5VLARRAYget_ndims( hid_t dataset_id,
			   hid_t type_id,
			   int *rank )
{
  hid_t       atom_type_id;
  H5T_class_t atom_class_id;

  /* Get the type of the atomic component */
  atom_type_id = H5Tget_super( type_id );

  /* Get the class of the atomic component. */
  atom_class_id = H5Tget_class( atom_type_id );

  /* Check whether the atom is an array class object or not */
  if ( atom_class_id == H5T_ARRAY) {
    /* Get rank */
    if ( (*rank = H5Tget_array_ndims( atom_type_id )) < 0 )
      goto out;
  }
  else {
    *rank = 0;		/* Means scalar values */
  }

 /* Terminate access to the datatypes */
 if ( H5Tclose( atom_type_id ) < 0 )
  goto out;

 return 0;

out:
 return -1;

}

/*-------------------------------------------------------------------------
 * Function: H5VLARRAYget_info
 *
 * Purpose: Gathers info about the VLEN type and other.
 *
 * Return: Success: 0, Failure: -1
 *
 * Programmer: Francesc Altet
 *
 * Date: November 19, 2003
 *
 *-------------------------------------------------------------------------
 */

herr_t H5VLARRAYget_info( hid_t   dataset_id,
			  hid_t   type_id,
			  hsize_t *nrecords,
			  hsize_t *base_dims,
			  hid_t   *base_type_id,
			  char    *base_byteorder )
{

  hid_t       space_id;
  H5T_class_t base_class_id;
  H5T_class_t atom_class_id;
  hid_t       atom_type_id;


  /* Get the dataspace handle */
  if ( (space_id = H5Dget_space( dataset_id )) < 0 )
    goto out;

  /* Get number of records (it should be rank-1) */
  if ( H5Sget_simple_extent_dims( space_id, nrecords, NULL) < 0 )
    goto out;

  /* Terminate access to the dataspace */
  if ( H5Sclose( space_id ) < 0 )
    goto out;

  /* Get the type of the atomic component */
  atom_type_id = H5Tget_super( type_id );

  /* Get the class of the atomic component. */
  atom_class_id = H5Tget_class( atom_type_id );

  /* Check whether the atom is an array class object or not */
  if ( atom_class_id == H5T_ARRAY) {
    /* Get the array base component */
    *base_type_id = H5Tget_super( atom_type_id );
    /* Get the class of base component */
    base_class_id = H5Tget_class( *base_type_id );
    /* Get dimensions */
    if ( H5Tget_array_dims(atom_type_id, base_dims, NULL) < 0 )
      goto out;
    /* Release the datatypes */
    if ( H5Tclose(atom_type_id ) )
      return -1;
  }
  else {
    base_class_id = atom_class_id;
    *base_type_id = atom_type_id;
    base_dims = NULL; 		/* Is a scalar */
  }

  /* Get the byteorder */
  /* Only integer, float and time classes can be byteordered */
  if ((base_class_id == H5T_INTEGER) || (base_class_id == H5T_FLOAT)
      || (base_class_id == H5T_BITFIELD) || (base_class_id == H5T_COMPOUND)
      || (base_class_id == H5T_TIME)) {
    get_order(*base_type_id, base_byteorder);
  }
  else {
    strcpy(base_byteorder, "non-relevant");
  }

  return 0;

out:
 return -1;

}
