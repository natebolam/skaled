#include <libskale/SnapshotManager.h>
#include <skutils/btrfs.h>

#include <test/tools/libtesteth/TestHelper.h>

#include <boost/filesystem.hpp>
#include <boost/test/unit_test.hpp>

#include <iostream>
#include <string>

#include <stdlib.h>

#include <sys/stat.h>
#include <sys/wait.h>

using namespace std;
namespace fs = boost::filesystem;

boost::unit_test::assertion_result option_all( boost::unit_test::test_unit_id ) {
    return boost::unit_test::assertion_result( dev::test::Options::get().all ? true : false );
}

int setid_system( const char* cmd, uid_t uid, gid_t gid ) {
    __pid_t pid = fork();
    if ( pid ) {
        int status;
        waitpid( pid, &status, 0 );
        return WEXITSTATUS( status );
    }

    setresuid( uid, uid, uid );
    setresgid( gid, gid, gid );

    execl( "/bin/sh", "sh", "-c", cmd, ( char* ) NULL );
    return 0;
}

struct FixtureCommon {
    const string BTRFS_FILE_PATH = "btrfs.file";
    const string BTRFS_DIR_PATH = "btrfs";
    uid_t sudo_uid;
    gid_t sudo_gid;

    void check_sudo() {
        char* id_str = getenv( "SUDO_UID" );
        if ( id_str == NULL ) {
            cerr << "Please run under sudo" << endl;
            exit( -1 );
        }

        sscanf( id_str, "%d", &sudo_uid );

        //    uid_t ru, eu, su;
        //    getresuid( &ru, &eu, &su );
        //    cerr << ru << " " << eu << " " << su << endl;

        if ( geteuid() != 0 ) {
            cerr << "Need to be root" << endl;
            exit( -1 );
        }

        id_str = getenv( "SUDO_GID" );
        sscanf( id_str, "%d", &sudo_gid );

        gid_t rgid, egid, sgid;
        getresgid( &rgid, &egid, &sgid );
        cerr << "GIDS: " << rgid << " " << egid << " " << sgid << endl;
    }

    void dropRoot() {
        int res = setresgid( sudo_gid, sudo_gid, 0 );
        cerr << "setresgid " << sudo_gid << " " << res << endl;
        if ( res < 0 )
            cerr << strerror( errno ) << endl;
        res = setresuid( sudo_uid, sudo_uid, 0 );
        cerr << "setresuid " << sudo_uid << " " << res << endl;
        if ( res < 0 )
            cerr << strerror( errno ) << endl;
    }

    void gainRoot() {
        int res = setresuid( 0, 0, 0 );
        if ( res ) {
            cerr << strerror( errno ) << endl;
            assert( false );
        }
        setresgid( 0, 0, 0 );
        if ( res ) {
            cerr << strerror( errno ) << endl;
            assert( false );
        }
    }
};

struct BtrfsFixture : public FixtureCommon {
    BtrfsFixture() {
        check_sudo();

        dropRoot();

        system( ( "dd if=/dev/zero of=" + BTRFS_FILE_PATH + " bs=1M count=200" ).c_str() );
        system( ( "mkfs.btrfs " + BTRFS_FILE_PATH ).c_str() );
        system( ( "mkdir " + BTRFS_DIR_PATH ).c_str() );

        gainRoot();
        system( ( "mount -o user_subvol_rm_allowed " + BTRFS_FILE_PATH + " " + BTRFS_DIR_PATH )
                    .c_str() );
        chown( BTRFS_DIR_PATH.c_str(), sudo_uid, sudo_gid );
        dropRoot();

        btrfs.subvolume.create( ( BTRFS_DIR_PATH + "/vol1" ).c_str() );
        btrfs.subvolume.create( ( BTRFS_DIR_PATH + "/vol2" ).c_str() );
        // system( ( "mkdir " + BTRFS_DIR_PATH + "/snapshots" ).c_str() );

        gainRoot();
    }

    ~BtrfsFixture() {
        const char* NC = getenv( "NC" );
        if ( NC )
            return;
        gainRoot();
        system( ( "umount " + BTRFS_DIR_PATH ).c_str() );
        system( ( "rmdir " + BTRFS_DIR_PATH ).c_str() );
        system( ( "rm " + BTRFS_FILE_PATH ).c_str() );
    }
};

struct NoBtrfsFixture : public FixtureCommon {
    NoBtrfsFixture() {
        check_sudo();
        dropRoot();
        system( ( "mkdir " + BTRFS_DIR_PATH ).c_str() );
        system( ( "mkdir " + BTRFS_DIR_PATH + "/vol1" ).c_str() );
        system( ( "mkdir " + BTRFS_DIR_PATH + "/vol2" ).c_str() );
        gainRoot();
    }
    ~NoBtrfsFixture() {
        gainRoot();
        system( ( "rm -rf " + BTRFS_DIR_PATH ).c_str() );
    }
};

BOOST_AUTO_TEST_SUITE( BtrfsTestSuite, *boost::unit_test::precondition( option_all ) )

BOOST_FIXTURE_TEST_CASE( SimplePositiveTest, BtrfsFixture ) {
    SnapshotManager mgr( fs::path( BTRFS_DIR_PATH ), {"vol1", "vol2"} );

    // add files 1
    fs::create_directory( fs::path( BTRFS_DIR_PATH ) / "vol1" / "d11" );
    fs::create_directory( fs::path( BTRFS_DIR_PATH ) / "vol2" / "d21" );
    BOOST_REQUIRE( fs::exists( fs::path( BTRFS_DIR_PATH ) / "vol1" / "d11" ) );
    BOOST_REQUIRE( fs::exists( fs::path( BTRFS_DIR_PATH ) / "vol2" / "d21" ) );

    // create snapshot 1 and check its presense
    mgr.doSnapshot( 1 );
    BOOST_REQUIRE( fs::exists( fs::path( BTRFS_DIR_PATH ) / "snapshots" / "1" / "vol1" / "d11" ) );
    BOOST_REQUIRE( fs::exists( fs::path( BTRFS_DIR_PATH ) / "snapshots" / "1" / "vol2" / "d21" ) );

    // add and remove something
    fs::create_directory( fs::path( BTRFS_DIR_PATH ) / "vol1" / "d12" );
    fs::remove( fs::path( BTRFS_DIR_PATH ) / "vol2" / "d21" );
    BOOST_REQUIRE( fs::exists( fs::path( BTRFS_DIR_PATH ) / "vol1" / "d12" ) );
    BOOST_REQUIRE( !fs::exists( fs::path( BTRFS_DIR_PATH ) / "vol2" / "d21" ) );

    // create snapshot 2 and check files 1 and files 2
    mgr.doSnapshot( 2 );
    BOOST_REQUIRE( fs::exists( fs::path( BTRFS_DIR_PATH ) / "snapshots" / "2" / "vol1" / "d11" ) );
    BOOST_REQUIRE( fs::exists( fs::path( BTRFS_DIR_PATH ) / "snapshots" / "2" / "vol1" / "d12" ) );
    BOOST_REQUIRE( !fs::exists( fs::path( BTRFS_DIR_PATH ) / "snapshots" / "2" / "vol2" / "d21" ) );

    // check that files appear/disappear on restore
    mgr.restoreSnapshot( 1 );
    BOOST_REQUIRE( fs::exists( fs::path( BTRFS_DIR_PATH ) / "vol1" / "d11" ) );
    BOOST_REQUIRE( fs::exists( fs::path( BTRFS_DIR_PATH ) / "vol2" / "d21" ) );
    BOOST_REQUIRE( !fs::exists( fs::path( BTRFS_DIR_PATH ) / "vol1" / "d12" ) );

    fs::path diff12 = mgr.makeDiff( 1, 2 );
    btrfs.subvolume._delete( ( BTRFS_DIR_PATH + "/snapshots/2/vol1" ).c_str() );
    btrfs.subvolume._delete( ( BTRFS_DIR_PATH + "/snapshots/2/vol2" ).c_str() );
    fs::remove_all( BTRFS_DIR_PATH + "/snapshots/2" );
    BOOST_REQUIRE( !fs::exists( fs::path( BTRFS_DIR_PATH ) / "snapshots" / "2" ) );

    mgr.importDiff( 2, diff12 );
    BOOST_REQUIRE( fs::exists( fs::path( BTRFS_DIR_PATH ) / "snapshots" / "2" / "vol1" / "d11" ) );
    BOOST_REQUIRE( fs::exists( fs::path( BTRFS_DIR_PATH ) / "snapshots" / "2" / "vol1" / "d12" ) );
    BOOST_REQUIRE( !fs::exists( fs::path( BTRFS_DIR_PATH ) / "snapshots" / "2" / "vol2" / "d21" ) );

    mgr.restoreSnapshot( 2 );
    BOOST_REQUIRE( fs::exists( fs::path( BTRFS_DIR_PATH ) / "vol1" / "d11" ) );
    BOOST_REQUIRE( fs::exists( fs::path( BTRFS_DIR_PATH ) / "vol1" / "d12" ) );
    BOOST_REQUIRE( !fs::exists( fs::path( BTRFS_DIR_PATH ) / "vol2" / "d21" ) );
}

BOOST_FIXTURE_TEST_CASE( NoBtrfsTest, NoBtrfsFixture ) {
    BOOST_REQUIRE_THROW( SnapshotManager mgr( fs::path( BTRFS_DIR_PATH ), {"vol1", "vol2"} ),
        SnapshotManager::CannotPerformBtrfsOperation );
}

BOOST_FIXTURE_TEST_CASE( BadPathTest, BtrfsFixture ) {
    BOOST_REQUIRE_EXCEPTION(
        SnapshotManager mgr( fs::path( BTRFS_DIR_PATH ) / "_invalid", {"vol1", "vol2"} ),
        SnapshotManager::InvalidPath, [this]( const SnapshotManager::InvalidPath& ex ) -> bool {
            return ex.path == fs::path( BTRFS_DIR_PATH ) / "_invalid";
        } );

    BOOST_REQUIRE_EXCEPTION(
        SnapshotManager mgr( fs::path( BTRFS_DIR_PATH ), {"vol1", "invalid3", "vol2"} ),
        SnapshotManager::InvalidPath, [this]( const SnapshotManager::InvalidPath& ex ) -> bool {
            return ex.path == fs::path( BTRFS_DIR_PATH ) / "invalid3";
        } );
}

BOOST_FIXTURE_TEST_CASE( InaccessiblePathTest, BtrfsFixture,
    *boost::unit_test::precondition( []( unsigned long ) -> bool { return false; } ) ) {
    fs::create_directory( fs::path( BTRFS_DIR_PATH ) / "_no_w" );
    chmod( ( BTRFS_DIR_PATH + "/_no_w" ).c_str(), 0775 );
    fs::create_directory( fs::path( BTRFS_DIR_PATH ) / "_no_w" / "vol1" );
    chmod( ( BTRFS_DIR_PATH + "/_no_w/vol1" ).c_str(), 0777 );

    fs::create_directory( fs::path( BTRFS_DIR_PATH ) / "_no_x" );
    chmod( ( BTRFS_DIR_PATH + "/_no_x" ).c_str(), 0774 );
    fs::create_directory( fs::path( BTRFS_DIR_PATH ) / "_no_x" / "vol1" );
    chmod( ( BTRFS_DIR_PATH + "/_no_x/vol1" ).c_str(), 0777 );

    fs::create_directory( fs::path( BTRFS_DIR_PATH ) / "_no_r" );
    chmod( ( BTRFS_DIR_PATH + "/_no_r" ).c_str(), 0770 );

    fs::create_directory( fs::path( BTRFS_DIR_PATH ) / "_no_x" / "_no_parent_x" );
    chmod( ( BTRFS_DIR_PATH + "/_no_x/_no_parent_x" ).c_str(), 0777 );

    fs::create_directory( fs::path( BTRFS_DIR_PATH ) / "_no_r" / "_no_parent_r" );
    chmod( ( BTRFS_DIR_PATH + "/_no_r/_no_parent_r" ).c_str(), 0777 );

    dropRoot();

    BOOST_REQUIRE_EXCEPTION( SnapshotManager mgr( fs::path( BTRFS_DIR_PATH ) / "_no_w", {"vol1"} ),
        SnapshotManager::CannotCreate, [this]( const SnapshotManager::CannotCreate& ex ) -> bool {
            return ex.path == fs::path( BTRFS_DIR_PATH ) / "_no_w" / "snapshots";
        } );

    BOOST_REQUIRE_EXCEPTION( SnapshotManager mgr( fs::path( BTRFS_DIR_PATH ) / "_no_x", {"vol1"} ),
        SnapshotManager::CannotCreate, [this]( const SnapshotManager::CannotCreate& ex ) -> bool {
            return ex.path == fs::path( BTRFS_DIR_PATH ) / "_no_x" / "snapshots";
        } );

    BOOST_REQUIRE_EXCEPTION( SnapshotManager mgr( fs::path( BTRFS_DIR_PATH ) / "_no_r", {"vol1"} ),
        SnapshotManager::CannotCreate, [this]( const SnapshotManager::CannotCreate& ex ) -> bool {
            return ex.path == fs::path( BTRFS_DIR_PATH ) / "_no_x" / "snapshots";
        } );
}

BOOST_FIXTURE_TEST_CASE( SnapshotTest, BtrfsFixture ) {
    SnapshotManager mgr( fs::path( BTRFS_DIR_PATH ), {"vol1", "vol2"} );

    BOOST_REQUIRE_NO_THROW( mgr.doSnapshot( 2 ) );
    BOOST_REQUIRE_THROW( mgr.doSnapshot( 2 ), SnapshotManager::SnapshotPresent );

    chmod( ( fs::path( BTRFS_DIR_PATH ) / "snapshots" ).c_str(), 0 );

    dropRoot();

    BOOST_REQUIRE_EXCEPTION( mgr.doSnapshot( 3 ), SnapshotManager::CannotRead,
        [this]( const SnapshotManager::CannotRead& ex ) -> bool {
            return ex.path == fs::path( BTRFS_DIR_PATH ) / "snapshots" / "3";
        } );

    gainRoot();
    chmod( ( fs::path( BTRFS_DIR_PATH ) / "snapshots" ).c_str(), 0111 );
    dropRoot();

    BOOST_REQUIRE_EXCEPTION( mgr.doSnapshot( 3 ), SnapshotManager::CannotCreate,
        [this]( const SnapshotManager::CannotCreate& ex ) -> bool {
            return ex.path == fs::path( BTRFS_DIR_PATH ) / "snapshots" / "3";
        } );

    // interesting that under normal user we still can do snapshot
    BOOST_REQUIRE_NO_THROW( mgr.restoreSnapshot( 2 ) );
}

BOOST_FIXTURE_TEST_CASE( RestoreTest, BtrfsFixture ) {
    SnapshotManager mgr( fs::path( BTRFS_DIR_PATH ), {"vol1", "vol2"} );

    BOOST_REQUIRE_THROW( mgr.restoreSnapshot( 2 ), SnapshotManager::SnapshotAbsent );

    BOOST_REQUIRE_NO_THROW( mgr.doSnapshot( 2 ) );

    BOOST_REQUIRE_NO_THROW( mgr.restoreSnapshot( 2 ) );

    BOOST_REQUIRE_EQUAL(
        0, btrfs.subvolume._delete(
               ( fs::path( BTRFS_DIR_PATH ) / "snapshots" / "2" / "vol1" ).c_str() ) );
    BOOST_REQUIRE_THROW( mgr.restoreSnapshot( 2 ), SnapshotManager::CannotPerformBtrfsOperation );
}

BOOST_FIXTURE_TEST_CASE( DiffTest, BtrfsFixture ) {
    SnapshotManager mgr( fs::path( BTRFS_DIR_PATH ), {"vol1", "vol2"} );
    mgr.doSnapshot( 2 );
    fs::create_directory( fs::path( BTRFS_DIR_PATH ) / "vol1" / "dir" );
    mgr.doSnapshot( 4 );

    BOOST_REQUIRE_THROW( mgr.makeDiff( 1, 3 ), SnapshotManager::SnapshotAbsent );
    BOOST_REQUIRE_THROW( mgr.makeDiff( 2, 3 ), SnapshotManager::SnapshotAbsent );
    BOOST_REQUIRE_THROW( mgr.makeDiff( 1, 2 ), SnapshotManager::SnapshotAbsent );

    fs::path tmp;
    BOOST_REQUIRE_NO_THROW( tmp = mgr.makeDiff( 2, 4 ) );
    fs::remove( tmp );

    BOOST_REQUIRE_NO_THROW( tmp = mgr.makeDiff( 2, 2 ) );
    fs::remove( tmp );

    BOOST_REQUIRE_NO_THROW( tmp = mgr.makeDiff( 4, 2 ) );
    fs::remove( tmp );

    // strange - but ok...
    BOOST_REQUIRE_NO_THROW( tmp = mgr.makeDiff( 2, 4 ) );
    BOOST_REQUIRE_GT( fs::file_size( tmp ), 0 );
    fs::remove( tmp );

    btrfs.subvolume._delete( ( fs::path( BTRFS_DIR_PATH ) / "snapshots" / "4" / "vol1" ).c_str() );

    BOOST_REQUIRE_THROW( tmp = mgr.makeDiff( 2, 4 ), SnapshotManager::CannotPerformBtrfsOperation );
}

// TODO Tests to check no files left in /tmp?!

BOOST_FIXTURE_TEST_CASE( ImportTest, BtrfsFixture ) {
    SnapshotManager mgr( fs::path( BTRFS_DIR_PATH ), {"vol1", "vol2"} );

    BOOST_REQUIRE_THROW( mgr.importDiff( 1, fs::path( BTRFS_DIR_PATH ) / "_nonexistent" ),
        SnapshotManager::InvalidPath );

    BOOST_REQUIRE_NO_THROW( mgr.doSnapshot( 2 ) );
    BOOST_REQUIRE_NO_THROW( mgr.doSnapshot( 4 ) );

    fs::path diff24;
    BOOST_REQUIRE_NO_THROW( diff24 = mgr.makeDiff( 2, 4 ) );

    BOOST_REQUIRE_THROW( mgr.importDiff( 4, diff24 ), SnapshotManager::SnapshotPresent );

    // delete dest
    btrfs.subvolume._delete( ( fs::path( BTRFS_DIR_PATH ) / "snapshots" / "4" / "vol1" ).c_str() );
    btrfs.subvolume._delete( ( fs::path( BTRFS_DIR_PATH ) / "snapshots" / "4" / "vol2" ).c_str() );
    fs::remove_all( fs::path( BTRFS_DIR_PATH ) / "snapshots" / "4" );

    BOOST_REQUIRE_NO_THROW( mgr.importDiff( 4, diff24 ) );

    // delete dest
    btrfs.subvolume._delete( ( fs::path( BTRFS_DIR_PATH ) / "snapshots" / "4" / "vol1" ).c_str() );
    btrfs.subvolume._delete( ( fs::path( BTRFS_DIR_PATH ) / "snapshots" / "4" / "vol2" ).c_str() );
    fs::remove_all( fs::path( BTRFS_DIR_PATH ) / "snapshots" / "4" );

    // no source
    btrfs.subvolume._delete( ( fs::path( BTRFS_DIR_PATH ) / "snapshots" / "2" / "vol1" ).c_str() );

    BOOST_REQUIRE_THROW(
        mgr.importDiff( 4, diff24 ), SnapshotManager::CannotPerformBtrfsOperation );

    btrfs.subvolume._delete( ( fs::path( BTRFS_DIR_PATH ) / "snapshots" / "2" / "vol2" ).c_str() );
    fs::remove_all( fs::path( BTRFS_DIR_PATH ) / "snapshots" / "2" );
    BOOST_REQUIRE_THROW(
        mgr.importDiff( 4, diff24 ), SnapshotManager::CannotPerformBtrfsOperation );
}

BOOST_AUTO_TEST_SUITE_END()