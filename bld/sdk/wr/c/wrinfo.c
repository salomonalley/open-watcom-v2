/****************************************************************************
*
*                            Open Watcom Project
*
*    Portions Copyright (c) 1983-2002 Sybase, Inc. All Rights Reserved.
*
*  ========================================================================
*
*    This file contains Original Code and/or Modifications of Original
*    Code as defined in and that are subject to the Sybase Open Watcom
*    Public License version 1.0 (the 'License'). You may not use this file
*    except in compliance with the License. BY USING THIS FILE YOU AGREE TO
*    ALL TERMS AND CONDITIONS OF THE LICENSE. A copy of the License is
*    provided with the Original Code and Modifications, and is also
*    available at www.sybase.com/developer/opensource.
*
*    The Original Code and all software distributed under the License are
*    distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
*    EXPRESS OR IMPLIED, AND SYBASE AND ALL CONTRIBUTORS HEREBY DISCLAIM
*    ALL SUCH WARRANTIES, INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF
*    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR
*    NON-INFRINGEMENT. Please see the License for the specific language
*    governing rights and limitations under the License.
*
*  ========================================================================
*
* Description:  WHEN YOU FIGURE OUT WHAT THIS FILE DOES, PLEASE
*               DESCRIBE IT HERE!
*
****************************************************************************/


#include <windows.h>
#include <stdlib.h>
#include <string.h>

#include "watcom.h"
#include "wrglbl.h"
#include "wrinfoi.h"
#include "wrmsg.h"

/****************************************************************************/
/* macro definitions                                                        */
/****************************************************************************/

/****************************************************************************/
/* static function prototypes                                               */
/****************************************************************************/

int WRCopyFileToTemp( WRInfo *info, char *filename )
{
    if( filename == NULL ) {
        return( FALSE );
    }

    if( !WRFileExists( filename ) ) {
        return( FALSE );
    }

    if( info->tmp_file == NULL ) {
        info->tmp_file = WRGetTempFileName( NULL );
        if( info->tmp_file == NULL ) {
            return( FALSE );
        }
    }

    if( !WRCopyFile( info->tmp_file, filename ) ) {
        WRPrintErrorMsg( WR_BACKUPFAILED, filename, info->tmp_file,
                         strerror( WRGetLastError() ) );
        return( FALSE );
    }

    return( TRUE );
}

WRInfo * WRAPI WRAllocWRInfo( void )
{
    WRInfo  *info;

    if( (info = (WRInfo *)WRMemAlloc( sizeof( WRInfo ) )) != NULL ) {
        memset( info, 0, sizeof( WRInfo ) );
    }

    return( info );
}

void WRAPI WRFreeWRInfo( WRInfo *info )
{
    if( info != NULL ) {
        if( info->file_name != NULL ) {
            WRMemFree( info->file_name );
        }
        if( info->save_name != NULL ) {
            WRMemFree( info->save_name );
        }
        if( info->internal_filename != NULL ) {
            WRMemFree( info->internal_filename );
        }
        if( info->tmp_file != NULL ) {
            if( WRFileExists( info->tmp_file ) ) {
                WRDeleteFile( info->tmp_file );
            }
            WRMemFree( info->tmp_file );
        }
        if( info->dir != NULL ) {
            WRFreeWResDirData( info->dir );
            WResFreeDir( info->dir );
        }
        WRMemFree( info );
    }
}

void WRAPI WRFreeWResDirData( WResDir dir )
{
    WResTypeNode  *tnode;
    WResResNode   *rnode;
    WResLangNode  *lnode;

    if( dir == NULL ) {
        return;
    }

    tnode = dir->Head;
    while( tnode != NULL ) {
        rnode = tnode->Head;
        while( rnode != NULL ) {
            lnode = rnode->Head;
            while( lnode != NULL ) {
                if( lnode->data != NULL ) {
                    WRMemFree( lnode->data );
                    lnode->data = NULL;
                }
                lnode = lnode->Next;
            }
            rnode = rnode->Next;
        }
        tnode = tnode->Next;
    }
}

int WRAPI WRCountZeroLengthResources( WResDir dir )
{
    WResTypeNode        *tnode;
    WResResNode         *rnode;
    WResLangNode        *lnode;
    int                 count;

    if( dir == NULL ) {
        return( 0 );
    }

    count = 0;

    tnode = dir->Head;
    while( tnode != NULL ) {
        rnode = tnode->Head;
        while( rnode != NULL ) {
            lnode = rnode->Head;
            while( lnode != NULL ) {
                if( lnode->Info.Length == 0 ) {
                    count++;
                }
                lnode = lnode->Next;
            }
            rnode = rnode->Next;
        }
        tnode = tnode->Next;
    }

    return( count );
}

int WRRelinkDir( WResDir dest, WResDir src )
{
    WResLangType    lt;
    WResTypeNode    *dtnode;
    WResResNode     *drnode;
    WResLangNode    *dlnode;
    WResTypeNode    *stnode;
    WResResNode     *srnode;
    WResLangNode    *slnode;

    if( dest == NULL || src == NULL ) {
        return( FALSE );
    }

    if( dest->NumTypes != src->NumTypes || dest->NumResources != src->NumResources ) {
        return( FALSE );
    }

    dtnode = dest->Head;
    if( dtnode != NULL ) {
        stnode = WRFindTypeNodeFromWResID( src, &dtnode->Info.TypeName );
    }
    while( dtnode != NULL && stnode != NULL ) {
        drnode = dtnode->Head;
        if( drnode != NULL ) {
            srnode = WRFindResNodeFromWResID( stnode, &drnode->Info.ResName );
        }
        while( drnode != NULL && srnode != NULL ) {
            dlnode = drnode->Head;
            if( dlnode != NULL ) {
                lt = dlnode->Info.lang;
                slnode = WRFindLangNodeFromLangType( srnode, &lt );
            }
            while( dlnode != NULL && slnode != NULL ) {
                if( dlnode->data == NULL ) {
                    dlnode->Info.Offset = slnode->Info.Offset;
                }
                dlnode = dlnode->Next;
                if( dlnode != NULL ) {
                    lt = dlnode->Info.lang;
                    slnode = WRFindLangNodeFromLangType( srnode, &lt );
                }
            }
            drnode = drnode->Next;
            if( drnode != NULL ) {
                srnode = WRFindResNodeFromWResID( stnode, &drnode->Info.ResName );
            }
        }
        dtnode = dtnode->Next;
        if( dtnode != NULL ) {
            stnode = WRFindTypeNodeFromWResID( src, &dtnode->Info.TypeName );
        }
    }

    return( TRUE );
}

// We really should not reread the file but, alas, time demands
// something a little less pristine.
int WRRelinkInfo( WRInfo *info )
{
    char        fn_path[_MAX_PATH];
    WRInfo      *tinfo;
    int         ok;

    tinfo = NULL;

    ok = (info != NULL);

    if( ok ) {
        if( info->internal_filename != NULL ) {
            WRGetInternalRESName( info->save_name, fn_path );
        } else {
            strcpy( fn_path, info->save_name );
        }
        ok = WRCopyFileToTemp( info, fn_path );
    }

    if( ok ) {
        tinfo = WRLoadResource( fn_path, info->save_type );
        ok = (tinfo != NULL);
    }

    if( ok ) {
        ok = WRRelinkDir( info->dir, tinfo->dir );
    }

    if( tinfo != NULL ) {
        WRFreeWRInfo( tinfo );
    }

    return( ok );
}

int WRAPI WRGetInternalRESName( char *filename, char *newname )
{
    char                fn_drive[_MAX_DRIVE];
    char                fn_dir[_MAX_DIR];
    char                fn_name[_MAX_FNAME];
    char                fn_ext[_MAX_EXT + 1];

    if( filename != NULL && newname != NULL ) {
        _splitpath( filename, fn_drive, fn_dir, fn_name, fn_ext );
        strcpy( fn_ext, ".res" );
        _makepath( newname, fn_drive, fn_dir, fn_name, fn_ext );
        return( TRUE );
    }

    return( FALSE );
}
