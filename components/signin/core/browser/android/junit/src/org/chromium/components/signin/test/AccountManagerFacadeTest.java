// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin.test;

import static org.robolectric.Shadows.shadowOf;

import android.accounts.Account;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.os.UserManager;
import android.support.test.filters.SmallTest;
import android.support.test.rule.UiThreadTestRule;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.CommandLine;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.signin.AccountManagerDelegateException;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.ProfileDataSource;
import org.chromium.components.signin.test.util.AccountHolder;
import org.chromium.components.signin.test.util.FakeAccountManagerDelegate;
import org.chromium.testing.local.CustomShadowAsyncTask;
import org.chromium.testing.local.CustomShadowUserManager;

import java.util.Arrays;
import java.util.HashSet;
import java.util.concurrent.atomic.AtomicInteger;

/**
 * Test class for {@link AccountManagerFacade}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE,
        shadows = {CustomShadowAsyncTask.class, CustomShadowUserManager.class})
public class AccountManagerFacadeTest {
    @Rule
    public UiThreadTestRule mRule = new UiThreadTestRule();

    private CustomShadowUserManager mShadowUserManager;
    private FakeAccountManagerDelegate mDelegate;
    private AccountManagerFacade mFacade;

    @Before
    public void setUp() throws Exception {
        Context context = RuntimeEnvironment.application;
        UserManager userManager = (UserManager) context.getSystemService(Context.USER_SERVICE);
        mShadowUserManager = (CustomShadowUserManager) shadowOf(userManager);

        mDelegate = new FakeAccountManagerDelegate(
                FakeAccountManagerDelegate.ENABLE_PROFILE_DATA_SOURCE);
        Assert.assertFalse(mDelegate.isRegisterObserversCalled());
        AccountManagerFacade.overrideAccountManagerFacadeForTests(mDelegate);
        Assert.assertTrue(mDelegate.isRegisterObserversCalled());
        mFacade = AccountManagerFacade.get();
    }

    private void setAccountRestrictionPatterns(String... patterns) {
        Bundle restrictions = new Bundle();
        restrictions.putStringArray(
                AccountManagerFacade.ACCOUNT_RESTRICTION_PATTERNS_KEY, patterns);
        mShadowUserManager.setApplicationRestrictions(
                RuntimeEnvironment.application.getPackageName(), restrictions);
        RuntimeEnvironment.application.sendBroadcast(
                new Intent(Intent.ACTION_APPLICATION_RESTRICTIONS_CHANGED));
    }

    private void clearAccountRestrictionPatterns() {
        mShadowUserManager.setApplicationRestrictions(
                RuntimeEnvironment.application.getPackageName(), new Bundle());
        RuntimeEnvironment.application.sendBroadcast(
                new Intent(Intent.ACTION_APPLICATION_RESTRICTIONS_CHANGED));
    }

    @Test
    @SmallTest
    public void testCanonicalAccount() {
        addTestAccount("test@gmail.com");

        Assert.assertTrue(mFacade.hasAccountForName("test@gmail.com"));
        Assert.assertTrue(mFacade.hasAccountForName("Test@gmail.com"));
        Assert.assertTrue(mFacade.hasAccountForName("te.st@gmail.com"));
    }

    // If this test starts flaking, please re-open crbug.com/568636 and make sure there is some sort
    // of stack trace or error message in that bug BEFORE disabling the test.
    @Test
    @SmallTest
    public void testNonCanonicalAccount() {
        addTestAccount("test.me@gmail.com");

        Assert.assertTrue(mFacade.hasAccountForName("test.me@gmail.com"));
        Assert.assertTrue(mFacade.hasAccountForName("testme@gmail.com"));
        Assert.assertTrue(mFacade.hasAccountForName("Testme@gmail.com"));
        Assert.assertTrue(mFacade.hasAccountForName("te.st.me@gmail.com"));
    }

    @Test
    @SmallTest
    public void testProfileDataSource() throws Throwable {
        String accountName = "test@gmail.com";
        addTestAccount(accountName);

        mRule.runOnUiThread(() -> {
            ProfileDataSource.ProfileData profileData = new ProfileDataSource.ProfileData(
                    accountName, null, "Test Full Name", "Test Given Name");

            ProfileDataSource profileDataSource = mDelegate.getProfileDataSource();
            Assert.assertNotNull(profileDataSource);
            mDelegate.setProfileData(accountName, profileData);
            Assert.assertArrayEquals(profileDataSource.getProfileDataMap().values().toArray(),
                    new ProfileDataSource.ProfileData[] {profileData});

            mDelegate.setProfileData(accountName, null);
            Assert.assertArrayEquals(profileDataSource.getProfileDataMap().values().toArray(),
                    new ProfileDataSource.ProfileData[0]);
        });
    }

    @Test
    @SmallTest
    public void testGetAccounts() throws AccountManagerDelegateException {
        Assert.assertArrayEquals(new Account[] {}, mFacade.getGoogleAccounts());

        Account account = addTestAccount("test@gmail.com");
        Assert.assertArrayEquals(new Account[] {account}, mFacade.getGoogleAccounts());

        Account account2 = addTestAccount("test2@gmail.com");
        Assert.assertArrayEquals(new Account[] {account, account2}, mFacade.getGoogleAccounts());

        Account account3 = addTestAccount("test3@gmail.com");
        Assert.assertArrayEquals(
                new Account[] {account, account2, account3}, mFacade.getGoogleAccounts());

        removeTestAccount(account2);
        Assert.assertArrayEquals(new Account[] {account, account3}, mFacade.getGoogleAccounts());
    }

    @Test
    @SmallTest
    public void testGetAccountsWithAccountPattern() throws AccountManagerDelegateException {
        setAccountRestrictionPatterns("*@example.com");
        Account account = addTestAccount("test@example.com");
        Assert.assertArrayEquals(new Account[] {account}, mFacade.getGoogleAccounts());

        addTestAccount("test@gmail.com"); // Doesn't match the pattern.
        Assert.assertArrayEquals(new Account[] {account}, mFacade.getGoogleAccounts());

        Account account2 = addTestAccount("test2@example.com");
        Assert.assertArrayEquals(new Account[] {account, account2}, mFacade.getGoogleAccounts());

        addTestAccount("test2@gmail.com"); // Doesn't match the pattern.
        Assert.assertArrayEquals(new Account[] {account, account2}, mFacade.getGoogleAccounts());

        removeTestAccount(account);
        Assert.assertArrayEquals(new Account[] {account2}, mFacade.getGoogleAccounts());
    }

    @Test
    @SmallTest
    public void testGetAccountsWithTwoAccountPatterns() throws AccountManagerDelegateException {
        setAccountRestrictionPatterns("test1@example.com", "test2@gmail.com");
        addTestAccount("test@gmail.com"); // Doesn't match the pattern.
        addTestAccount("test@example.com"); // Doesn't match the pattern.
        Assert.assertArrayEquals(new Account[] {}, mFacade.getGoogleAccounts());

        Account account = addTestAccount("test1@example.com");
        Assert.assertArrayEquals(new Account[] {account}, mFacade.getGoogleAccounts());

        addTestAccount("test2@example.com"); // Doesn't match the pattern.
        Assert.assertArrayEquals(new Account[] {account}, mFacade.getGoogleAccounts());

        Account account2 = addTestAccount("test2@gmail.com");
        Assert.assertArrayEquals(new Account[] {account, account2}, mFacade.getGoogleAccounts());
    }

    @Test
    @SmallTest
    public void testGetAccountsWithAccountPatternsChange() throws AccountManagerDelegateException {
        Assert.assertArrayEquals(new Account[] {}, mFacade.getGoogleAccounts());

        Account account = addTestAccount("test@gmail.com");
        Assert.assertArrayEquals(new Account[] {account}, mFacade.getGoogleAccounts());

        Account account2 = addTestAccount("test2@example.com");
        Assert.assertArrayEquals(new Account[] {account, account2}, mFacade.getGoogleAccounts());

        Account account3 = addTestAccount("test3@gmail.com");
        Assert.assertArrayEquals(
                new Account[] {account, account2, account3}, mFacade.getGoogleAccounts());

        setAccountRestrictionPatterns("test@gmail.com");
        Assert.assertArrayEquals(new Account[] {account}, mFacade.getGoogleAccounts());

        setAccountRestrictionPatterns("*@example.com", "test3@gmail.com");
        Assert.assertArrayEquals(new Account[] {account2, account3}, mFacade.getGoogleAccounts());

        removeTestAccount(account3);
        Assert.assertArrayEquals(new Account[] {account2}, mFacade.getGoogleAccounts());

        clearAccountRestrictionPatterns();
        Assert.assertArrayEquals(new Account[] {account, account2}, mFacade.getGoogleAccounts());
    }

    @Test
    @SmallTest
    public void testGetAccountsMultipleMatchingPatterns() throws AccountManagerDelegateException {
        setAccountRestrictionPatterns("*@gmail.com", "test@gmail.com");
        Account account = addTestAccount("test@gmail.com"); // Matches both patterns
        Assert.assertArrayEquals(new Account[] {account}, mFacade.getGoogleAccounts());
    }

    @Test
    @SmallTest
    public void testCheckChildAccount_usmSwitchOff() throws AccountManagerDelegateException {
        Account testAccount = addTestAccount("test@gmail.com");
        Account ucaAccount =
                addTestAccount("uca@gmail.com", AccountManagerFacade.FEATURE_IS_CHILD_ACCOUNT_KEY);
        Account usmAccount =
                addTestAccount("usm@gmail.com", AccountManagerFacade.FEATURE_IS_USM_ACCOUNT_KEY);
        Account bothAccount = addTestAccount("uca_usm@gmail.com",
                AccountManagerFacade.FEATURE_IS_CHILD_ACCOUNT_KEY,
                AccountManagerFacade.FEATURE_IS_USM_ACCOUNT_KEY);

        assertChildAccountStatus(testAccount, false);
        assertChildAccountStatus(ucaAccount, true);
        assertChildAccountStatus(usmAccount, false);
        assertChildAccountStatus(bothAccount, true);
    }

    @Test
    @SmallTest
    public void testCheckChildAccount_usmSwitchOn() throws AccountManagerDelegateException {
        Account testAccount = addTestAccount("test@gmail.com");
        Account ucaAccount =
                addTestAccount("uca@gmail.com", AccountManagerFacade.FEATURE_IS_CHILD_ACCOUNT_KEY);
        Account usmAccount =
                addTestAccount("usm@gmail.com", AccountManagerFacade.FEATURE_IS_USM_ACCOUNT_KEY);
        Account bothAccount = addTestAccount("uca_usm@gmail.com",
                AccountManagerFacade.FEATURE_IS_CHILD_ACCOUNT_KEY,
                AccountManagerFacade.FEATURE_IS_USM_ACCOUNT_KEY);

        CommandLine.getInstance().appendSwitch(AccountManagerFacade.ENABLE_USM_ACCOUNTS_SWITCH);

        assertChildAccountStatus(testAccount, false);
        assertChildAccountStatus(ucaAccount, true);
        assertChildAccountStatus(usmAccount, true);
        assertChildAccountStatus(bothAccount, true);
    }

    private Account addTestAccount(String accountName, String... features) {
        Account account = AccountManagerFacade.createAccountFromName(accountName);
        AccountHolder holder = AccountHolder.builder(account)
                                       .alwaysAccept(true)
                                       .featureSet(new HashSet<>(Arrays.asList(features)))
                                       .build();
        mDelegate.addAccountHolderExplicitly(holder);
        Assert.assertFalse(AccountManagerFacade.get().isUpdatePending());
        return account;
    }

    private void removeTestAccount(Account account) {
        mDelegate.removeAccountHolderExplicitly(AccountHolder.builder(account).build());
    }

    private void assertChildAccountStatus(Account account, boolean status) {
        final AtomicInteger callCount = new AtomicInteger();
        AccountManagerFacade.get().checkChildAccount(account, new Callback<Boolean>() {
            @Override
            public void onResult(Boolean result) {
                callCount.incrementAndGet();
                Assert.assertEquals(result, status);
            }
        });
        Assert.assertEquals(1, callCount.get());
    }
}
