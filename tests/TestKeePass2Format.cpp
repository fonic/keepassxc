/*
 *  Copyright (C) 2018 KeePassXC Team <team@keepassxc.org>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 or (at your option)
 *  version 3 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "TestKeePass2Format.h"

#include "core/Group.h"
#include "core/Metadata.h"
#include "crypto/Crypto.h"
#include "keys/PasswordKey.h"
#include "format/KdbxXmlReader.h"

#include "FailDevice.h"
#include "config-keepassx-tests.h"

#include <QFile>
#include <QTest>

void TestKeePass2Format::initTestCase()
{
    QVERIFY(Crypto::init());

    // read raw XML database
    bool hasError;
    QString errorString;
    m_xmlDb.reset(readXml(QString(KEEPASSX_TEST_DATA_DIR).append("/NewDatabase.xml"), true, hasError, errorString));
    if (hasError) {
        QFAIL(qPrintable(QString("Error while reading XML: ").append(errorString)));
    }
    QVERIFY(m_xmlDb.data());

    // construct and write KDBX to buffer
    CompositeKey key;
    key.addKey(PasswordKey("test"));

    m_kdbxSourceDb.reset(new Database());
    m_kdbxSourceDb->setKey(key);
    m_kdbxSourceDb->metadata()->setName("TESTDB");
    Group* group = m_kdbxSourceDb->rootGroup();
    group->setUuid(Uuid::random());
    group->setNotes("I'm a note!");
    auto entry = new Entry();
    entry->setPassword(QString::fromUtf8("\xc3\xa4\xa3\xb6\xc3\xbc\xe9\x9b\xbb\xe7\xb4\x85"));
    entry->setUuid(Uuid::random());
    entry->attributes()->set("test", "protectedTest", true);
    QVERIFY(entry->attributes()->isProtected("test"));
    entry->attachments()->set("myattach.txt", QByteArray("this is an attachment"));
    entry->attachments()->set("aaa.txt", QByteArray("also an attachment"));
    entry->setGroup(group);
    auto groupNew = new Group();
    groupNew->setUuid(Uuid::random());
    groupNew->setName("TESTGROUP");
    groupNew->setNotes("I'm a sub group note!");
    groupNew->setParent(group);

    m_kdbxTargetBuffer.open(QBuffer::ReadWrite);
    writeKdbx(&m_kdbxTargetBuffer, m_kdbxSourceDb.data(), hasError, errorString);
    if (hasError) {
        QFAIL(qPrintable(QString("Error while writing database: ").append(errorString)));
    }

    // call sub class init method
    initTestCaseImpl();
}

void TestKeePass2Format::testXmlMetadata()
{
    QCOMPARE(m_xmlDb->metadata()->generator(), QString("KeePass"));
    QCOMPARE(m_xmlDb->metadata()->name(), QString("ANAME"));
    QCOMPARE(m_xmlDb->metadata()->nameChanged(), genDT(2010, 8, 8, 17, 24, 53));
    QCOMPARE(m_xmlDb->metadata()->description(), QString("ADESC"));
    QCOMPARE(m_xmlDb->metadata()->descriptionChanged(), genDT(2010, 8, 8, 17, 27, 12));
    QCOMPARE(m_xmlDb->metadata()->defaultUserName(), QString("DEFUSERNAME"));
    QCOMPARE(m_xmlDb->metadata()->defaultUserNameChanged(), genDT(2010, 8, 8, 17, 27, 45));
    QCOMPARE(m_xmlDb->metadata()->maintenanceHistoryDays(), 127);
    QCOMPARE(m_xmlDb->metadata()->color(), QColor(0xff, 0xef, 0x00));
    QCOMPARE(m_xmlDb->metadata()->masterKeyChanged(), genDT(2012, 4, 5, 17, 9, 34));
    QCOMPARE(m_xmlDb->metadata()->masterKeyChangeRec(), 101);
    QCOMPARE(m_xmlDb->metadata()->masterKeyChangeForce(), -1);
    QCOMPARE(m_xmlDb->metadata()->protectTitle(), false);
    QCOMPARE(m_xmlDb->metadata()->protectUsername(), true);
    QCOMPARE(m_xmlDb->metadata()->protectPassword(), false);
    QCOMPARE(m_xmlDb->metadata()->protectUrl(), true);
    QCOMPARE(m_xmlDb->metadata()->protectNotes(), false);
    QCOMPARE(m_xmlDb->metadata()->recycleBinEnabled(), true);
    QVERIFY(m_xmlDb->metadata()->recycleBin() != nullptr);
    QCOMPARE(m_xmlDb->metadata()->recycleBin()->name(), QString("Recycle Bin"));
    QCOMPARE(m_xmlDb->metadata()->recycleBinChanged(), genDT(2010, 8, 25, 16, 12, 57));
    QVERIFY(m_xmlDb->metadata()->entryTemplatesGroup() == nullptr);
    QCOMPARE(m_xmlDb->metadata()->entryTemplatesGroupChanged(), genDT(2010, 8, 8, 17, 24, 19));
    QVERIFY(m_xmlDb->metadata()->lastSelectedGroup() != nullptr);
    QCOMPARE(m_xmlDb->metadata()->lastSelectedGroup()->name(), QString("NewDatabase"));
    QVERIFY(m_xmlDb->metadata()->lastTopVisibleGroup() == m_xmlDb->metadata()->lastSelectedGroup());
    QCOMPARE(m_xmlDb->metadata()->historyMaxItems(), -1);
    QCOMPARE(m_xmlDb->metadata()->historyMaxSize(), 5242880);
}

void TestKeePass2Format::testXmlCustomIcons()
{
    QCOMPARE(m_xmlDb->metadata()->customIcons().size(), 1);
    Uuid uuid = Uuid::fromBase64("++vyI+daLk6omox4a6kQGA==");
    QVERIFY(m_xmlDb->metadata()->customIcons().contains(uuid));
    QImage icon = m_xmlDb->metadata()->customIcon(uuid);
    QCOMPARE(icon.width(), 16);
    QCOMPARE(icon.height(), 16);

    for (int x = 0; x < 16; x++) {
        for (int y = 0; y < 16; y++) {
            QRgb rgb = icon.pixel(x, y);
            QCOMPARE(qRed(rgb), 128);
            QCOMPARE(qGreen(rgb), 0);
            QCOMPARE(qBlue(rgb), 128);
        }
    }
}

void TestKeePass2Format::testXmlCustomData()
{
    QHash<QString, QString> customFields = m_xmlDb->metadata()->customFields();

    QCOMPARE(customFields.size(), 2);
    QCOMPARE(customFields.value("A Sample Test Key"), QString("valu"));
    QCOMPARE(customFields.value("custom key"), QString("blub"));
}

void TestKeePass2Format::testXmlGroupRoot()
{
    const Group* group = m_xmlDb->rootGroup();
    QVERIFY(group);
    QCOMPARE(group->uuid().toBase64(), QString("lmU+9n0aeESKZvcEze+bRg=="));
    QCOMPARE(group->name(), QString("NewDatabase"));
    QCOMPARE(group->notes(), QString(""));
    QCOMPARE(group->iconNumber(), 49);
    QCOMPARE(group->iconUuid(), Uuid());
    QVERIFY(group->isExpanded());
    TimeInfo ti = group->timeInfo();
    QCOMPARE(ti.lastModificationTime(), genDT(2010, 8, 8, 17, 24, 27));
    QCOMPARE(ti.creationTime(), genDT(2010, 8, 7, 17, 24, 27));
    QCOMPARE(ti.lastAccessTime(), genDT(2010, 8, 9, 9, 9, 44));
    QCOMPARE(ti.expiryTime(), genDT(2010, 8, 8, 17, 24, 17));
    QVERIFY(!ti.expires());
    QCOMPARE(ti.usageCount(), 52);
    QCOMPARE(ti.locationChanged(), genDT(2010, 8, 8, 17, 24, 27));
    QCOMPARE(group->defaultAutoTypeSequence(), QString(""));
    QCOMPARE(group->autoTypeEnabled(), Group::Inherit);
    QCOMPARE(group->searchingEnabled(), Group::Inherit);
    QCOMPARE(group->lastTopVisibleEntry()->uuid().toBase64(), QString("+wSUOv6qf0OzW8/ZHAs2sA=="));

    QCOMPARE(group->children().size(), 3);
    QVERIFY(m_xmlDb->metadata()->recycleBin() == m_xmlDb->rootGroup()->children().at(2));

    QCOMPARE(group->entries().size(), 2);
}

void TestKeePass2Format::testXmlGroup1()
{
    const Group* group = m_xmlDb->rootGroup()->children().at(0);

    QCOMPARE(group->uuid().toBase64(), QString("AaUYVdXsI02h4T1RiAlgtg=="));
    QCOMPARE(group->name(), QString("General"));
    QCOMPARE(group->notes(), QString("Group Notez"));
    QCOMPARE(group->iconNumber(), 48);
    QCOMPARE(group->iconUuid(), Uuid());
    QCOMPARE(group->isExpanded(), true);
    QCOMPARE(group->defaultAutoTypeSequence(), QString("{Password}{ENTER}"));
    QCOMPARE(group->autoTypeEnabled(), Group::Enable);
    QCOMPARE(group->searchingEnabled(), Group::Disable);
    QVERIFY(!group->lastTopVisibleEntry());
}

void TestKeePass2Format::testXmlGroup2()
{
    const Group* group = m_xmlDb->rootGroup()->children().at(1);

    QCOMPARE(group->uuid().toBase64(), QString("1h4NtL5DK0yVyvaEnN//4A=="));
    QCOMPARE(group->name(), QString("Windows"));
    QCOMPARE(group->isExpanded(), false);

    QCOMPARE(group->children().size(), 1);
    const Group* child = group->children().first();

    QCOMPARE(child->uuid().toBase64(), QString("HoYE/BjLfUSW257pCHJ/eA=="));
    QCOMPARE(child->name(), QString("Subsub"));
    QCOMPARE(child->entries().size(), 1);

    const Entry* entry = child->entries().first();
    QCOMPARE(entry->uuid().toBase64(), QString("GZpdQvGXOU2kaKRL/IVAGg=="));
    QCOMPARE(entry->title(), QString("Subsub Entry"));
}

void TestKeePass2Format::testXmlEntry1()
{
    const Entry* entry = m_xmlDb->rootGroup()->entries().at(0);

    QCOMPARE(entry->uuid().toBase64(), QString("+wSUOv6qf0OzW8/ZHAs2sA=="));
    QCOMPARE(entry->historyItems().size(), 2);
    QCOMPARE(entry->iconNumber(), 0);
    QCOMPARE(entry->iconUuid(), Uuid());
    QVERIFY(!entry->foregroundColor().isValid());
    QVERIFY(!entry->backgroundColor().isValid());
    QCOMPARE(entry->overrideUrl(), QString(""));
    QCOMPARE(entry->tags(), QString("a b c"));

    const TimeInfo ti = entry->timeInfo();
    QCOMPARE(ti.lastModificationTime(), genDT(2010, 8, 25, 16, 19, 25));
    QCOMPARE(ti.creationTime(), genDT(2010, 8, 25, 16, 13, 54));
    QCOMPARE(ti.lastAccessTime(), genDT(2010, 8, 25, 16, 19, 25));
    QCOMPARE(ti.expiryTime(), genDT(2010, 8, 25, 16, 12, 57));
    QVERIFY(!ti.expires());
    QCOMPARE(ti.usageCount(), 8);
    QCOMPARE(ti.locationChanged(), genDT(2010, 8, 25, 16, 13, 54));

    QList<QString> attrs = entry->attributes()->keys();
    QCOMPARE(entry->attributes()->value("Notes"), QString("Notes"));
    QVERIFY(!entry->attributes()->isProtected("Notes"));
    QVERIFY(attrs.removeOne("Notes"));
    QCOMPARE(entry->attributes()->value("Password"), QString("Password"));
    QVERIFY(!entry->attributes()->isProtected("Password"));
    QVERIFY(attrs.removeOne("Password"));
    QCOMPARE(entry->attributes()->value("Title"), QString("Sample Entry 1"));
    QVERIFY(!entry->attributes()->isProtected("Title"));
    QVERIFY(attrs.removeOne("Title"));
    QCOMPARE(entry->attributes()->value("URL"), QString(""));
    QVERIFY(entry->attributes()->isProtected("URL"));
    QVERIFY(attrs.removeOne("URL"));
    QCOMPARE(entry->attributes()->value("UserName"), QString("User Name"));
    QVERIFY(entry->attributes()->isProtected("UserName"));
    QVERIFY(attrs.removeOne("UserName"));
    QVERIFY(attrs.isEmpty());

    QCOMPARE(entry->title(), entry->attributes()->value("Title"));
    QCOMPARE(entry->url(), entry->attributes()->value("URL"));
    QCOMPARE(entry->username(), entry->attributes()->value("UserName"));
    QCOMPARE(entry->password(), entry->attributes()->value("Password"));
    QCOMPARE(entry->notes(), entry->attributes()->value("Notes"));

    QCOMPARE(entry->attachments()->keys().size(), 1);
    QCOMPARE(entry->attachments()->value("myattach.txt"), QByteArray("abcdefghijk"));
    QCOMPARE(entry->historyItems().at(0)->attachments()->keys().size(), 1);
    QCOMPARE(entry->historyItems().at(0)->attachments()->value("myattach.txt"), QByteArray("0123456789"));
    QCOMPARE(entry->historyItems().at(1)->attachments()->keys().size(), 1);
    QCOMPARE(entry->historyItems().at(1)->attachments()->value("myattach.txt"), QByteArray("abcdefghijk"));

    QCOMPARE(entry->autoTypeEnabled(), false);
    QCOMPARE(entry->autoTypeObfuscation(), 0);
    QCOMPARE(entry->defaultAutoTypeSequence(), QString(""));
    QCOMPARE(entry->autoTypeAssociations()->size(), 1);
    const AutoTypeAssociations::Association assoc1 = entry->autoTypeAssociations()->get(0);
    QCOMPARE(assoc1.window, QString("Target Window"));
    QCOMPARE(assoc1.sequence, QString(""));
}

void TestKeePass2Format::testXmlEntry2()
{
    const Entry* entry = m_xmlDb->rootGroup()->entries().at(1);

    QCOMPARE(entry->uuid().toBase64(), QString("4jbADG37hkiLh2O0qUdaOQ=="));
    QCOMPARE(entry->iconNumber(), 0);
    QCOMPARE(entry->iconUuid().toBase64(), QString("++vyI+daLk6omox4a6kQGA=="));
    // TODO: test entry->icon()
    QCOMPARE(entry->foregroundColor(), QColor(255, 0, 0));
    QCOMPARE(entry->backgroundColor(), QColor(255, 255, 0));
    QCOMPARE(entry->overrideUrl(), QString("http://override.net/"));
    QCOMPARE(entry->tags(), QString(""));

    const TimeInfo ti = entry->timeInfo();
    QCOMPARE(ti.usageCount(), 7);

    QList<QString> attrs = entry->attributes()->keys();
    QCOMPARE(entry->attributes()->value("CustomString"), QString("isavalue"));
    QVERIFY(attrs.removeOne("CustomString"));
    QCOMPARE(entry->attributes()->value("Notes"), QString(""));
    QVERIFY(attrs.removeOne("Notes"));
    QCOMPARE(entry->attributes()->value("Password"), QString("Jer60Hz8o9XHvxBGcRqT"));
    QVERIFY(attrs.removeOne("Password"));
    QCOMPARE(entry->attributes()->value("Protected String"), QString("y")); // TODO: should have a protection attribute
    QVERIFY(attrs.removeOne("Protected String"));
    QCOMPARE(entry->attributes()->value("Title"), QString("Sample Entry 2"));
    QVERIFY(attrs.removeOne("Title"));
    QCOMPARE(entry->attributes()->value("URL"), QString("http://www.keepassx.org/"));
    QVERIFY(attrs.removeOne("URL"));
    QCOMPARE(entry->attributes()->value("UserName"), QString("notDEFUSERNAME"));
    QVERIFY(attrs.removeOne("UserName"));
    QVERIFY(attrs.isEmpty());

    QCOMPARE(entry->attachments()->keys().size(), 1);
    QCOMPARE(QString::fromLatin1(entry->attachments()->value("myattach.txt")), QString("abcdefghijk"));

    QCOMPARE(entry->autoTypeEnabled(), true);
    QCOMPARE(entry->autoTypeObfuscation(), 1);
    QCOMPARE(entry->defaultAutoTypeSequence(), QString("{USERNAME}{TAB}{PASSWORD}{ENTER}"));
    QCOMPARE(entry->autoTypeAssociations()->size(), 2);
    const AutoTypeAssociations::Association assoc1 = entry->autoTypeAssociations()->get(0);
    QCOMPARE(assoc1.window, QString("Target Window"));
    QCOMPARE(assoc1.sequence, QString("{Title}{UserName}"));
    const AutoTypeAssociations::Association assoc2 = entry->autoTypeAssociations()->get(1);
    QCOMPARE(assoc2.window, QString("Target Window 2"));
    QCOMPARE(assoc2.sequence, QString("{Title}{UserName} test"));
}

void TestKeePass2Format::testXmlEntryHistory()
{
    const Entry* entryMain = m_xmlDb->rootGroup()->entries().at(0);
    QCOMPARE(entryMain->historyItems().size(), 2);

    {
        const Entry* entry = entryMain->historyItems().at(0);
        QCOMPARE(entry->uuid(), entryMain->uuid());
        QVERIFY(!entry->parent());
        QCOMPARE(entry->timeInfo().lastModificationTime(), genDT(2010, 8, 25, 16, 13, 54));
        QCOMPARE(entry->timeInfo().usageCount(), 3);
        QCOMPARE(entry->title(), QString("Sample Entry"));
        QCOMPARE(entry->url(), QString("http://www.somesite.com/"));
    }

    {
        const Entry* entry = entryMain->historyItems().at(1);
        QCOMPARE(entry->uuid(), entryMain->uuid());
        QVERIFY(!entry->parent());
        QCOMPARE(entry->timeInfo().lastModificationTime(), genDT(2010, 8, 25, 16, 15, 43));
        QCOMPARE(entry->timeInfo().usageCount(), 7);
        QCOMPARE(entry->title(), QString("Sample Entry 1"));
        QCOMPARE(entry->url(), QString("http://www.somesite.com/"));
    }
}

void TestKeePass2Format::testXmlDeletedObjects()
{
    QList<DeletedObject> objList = m_xmlDb->deletedObjects();
    DeletedObject delObj;

    delObj = objList.takeFirst();
    QCOMPARE(delObj.uuid.toBase64(), QString("5K/bzWCSmkCv5OZxYl4N/w=="));
    QCOMPARE(delObj.deletionTime, genDT(2010, 8, 25, 16, 14, 12));

    delObj = objList.takeFirst();
    QCOMPARE(delObj.uuid.toBase64(), QString("80h8uSNWgkKhKCp1TgXF7g=="));
    QCOMPARE(delObj.deletionTime, genDT(2010, 8, 25, 16, 14, 14));

    QVERIFY(objList.isEmpty());
}

void TestKeePass2Format::testXmlBroken()
{
    QFETCH(QString, baseName);
    QFETCH(bool, strictMode);
    QFETCH(bool, expectError);

    QString xmlFile = QString("%1/%2.xml").arg(KEEPASSX_TEST_DATA_DIR, baseName);
    QVERIFY(QFile::exists(xmlFile));
    bool hasError;
    QString errorString;
    QScopedPointer<Database> db(readXml(xmlFile, strictMode, hasError, errorString));
    if (hasError) {
        qWarning("Reader error: %s", qPrintable(errorString));
    }
    QCOMPARE(hasError, expectError);
}

void TestKeePass2Format::testXmlBroken_data()
{
    QTest::addColumn<QString>("baseName");
    QTest::addColumn<bool>("strictMode");
    QTest::addColumn<bool>("expectError");

    //                                                                testfile                            strict?  error?
    QTest::newRow("BrokenNoGroupUuid                   (strict)") << "BrokenNoGroupUuid"               << true  << true;
    QTest::newRow("BrokenNoGroupUuid               (not strict)") << "BrokenNoGroupUuid"               << false << false;
    QTest::newRow("BrokenNoEntryUuid                   (strict)") << "BrokenNoEntryUuid"               << true  << true;
    QTest::newRow("BrokenNoEntryUuid               (not strict)") << "BrokenNoEntryUuid"               << false << false;
    QTest::newRow("BrokenNoRootGroup                   (strict)") << "BrokenNoRootGroup"               << true  << true;
    QTest::newRow("BrokenNoRootGroup               (not strict)") << "BrokenNoRootGroup"               << false << true;
    QTest::newRow("BrokenTwoRoots                      (strict)") << "BrokenTwoRoots"                  << true  << true;
    QTest::newRow("BrokenTwoRoots                  (not strict)") << "BrokenTwoRoots"                  << false << true;
    QTest::newRow("BrokenTwoRootGroups                 (strict)") << "BrokenTwoRootGroups"             << true  << true;
    QTest::newRow("BrokenTwoRootGroups             (not strict)") << "BrokenTwoRootGroups"             << false << true;
    QTest::newRow("BrokenGroupReference                (strict)") << "BrokenGroupReference"            << true  << false;
    QTest::newRow("BrokenGroupReference            (not strict)") << "BrokenGroupReference"            << false << false;
    QTest::newRow("BrokenDeletedObjects                (strict)") << "BrokenDeletedObjects"            << true  << true;
    QTest::newRow("BrokenDeletedObjects            (not strict)") << "BrokenDeletedObjects"            << false << false;
    QTest::newRow("BrokenDifferentEntryHistoryUuid     (strict)") << "BrokenDifferentEntryHistoryUuid" << true  << true;
    QTest::newRow("BrokenDifferentEntryHistoryUuid (not strict)") << "BrokenDifferentEntryHistoryUuid" << false << false;
}

void TestKeePass2Format::testXmlEmptyUuids()
{

    QString xmlFile = QString("%1/%2.xml").arg(KEEPASSX_TEST_DATA_DIR, "EmptyUuids");
    QVERIFY(QFile::exists(xmlFile));
    bool hasError;
    QString errorString;
    QScopedPointer<Database> dbp(readXml(xmlFile, true, hasError, errorString));
    if (hasError) {
        qWarning("Reader error: %s", qPrintable(errorString));
    }
    QVERIFY(!hasError);
}

void TestKeePass2Format::testXmlInvalidXmlChars()
{
    QScopedPointer<Database> dbWrite(new Database());

    QString strPlainInvalid = QString().append(QChar(0x02)).append(QChar(0x19))
            .append(QChar(0xFFFE)).append(QChar(0xFFFF));
    QString strPlainValid = QString().append(QChar(0x09)).append(QChar(0x0A))
            .append(QChar(0x20)).append(QChar(0xD7FF))
            .append(QChar(0xE000)).append(QChar(0xFFFD));
    // U+10437 in UTF-16: D801 DC37
    //                    high low  surrogate
    QString strSingleHighSurrogate1 = QString().append(QChar(0xD801));
    QString strSingleHighSurrogate2 = QString().append(QChar(0x31)).append(QChar(0xD801)).append(QChar(0x32));
    QString strHighHighSurrogate = QString().append(QChar(0xD801)).append(QChar(0xD801));
    QString strSingleLowSurrogate1 = QString().append(QChar(0xDC37));
    QString strSingleLowSurrogate2 = QString().append(QChar((0x31))).append(QChar(0xDC37)).append(QChar(0x32));
    QString strLowLowSurrogate = QString().append(QChar(0xDC37)).append(QChar(0xDC37));
    QString strSurrogateValid1 = QString().append(QChar(0xD801)).append(QChar(0xDC37));
    QString strSurrogateValid2 = QString().append(QChar(0x31)).append(QChar(0xD801)).append(QChar(0xDC37))
            .append(QChar(0x32));

    auto entry = new Entry();
    entry->setUuid(Uuid::random());
    entry->setGroup(dbWrite->rootGroup());
    entry->attributes()->set("PlainInvalid", strPlainInvalid);
    entry->attributes()->set("PlainValid", strPlainValid);
    entry->attributes()->set("SingleHighSurrogate1", strSingleHighSurrogate1);
    entry->attributes()->set("SingleHighSurrogate2", strSingleHighSurrogate2);
    entry->attributes()->set("HighHighSurrogate", strHighHighSurrogate);
    entry->attributes()->set("SingleLowSurrogate1", strSingleLowSurrogate1);
    entry->attributes()->set("SingleLowSurrogate2", strSingleLowSurrogate2);
    entry->attributes()->set("LowLowSurrogate", strLowLowSurrogate);
    entry->attributes()->set("SurrogateValid1", strSurrogateValid1);
    entry->attributes()->set("SurrogateValid2", strSurrogateValid2);

    QBuffer buffer;
    buffer.open(QIODevice::ReadWrite);
    bool hasError;
    QString errorString;
    writeXml(&buffer, dbWrite.data(), hasError, errorString);
    QVERIFY(!hasError);
    buffer.seek(0);

    QScopedPointer<Database> dbRead(readXml(&buffer, true, hasError, errorString));
    if (hasError) {
        qWarning("Database read error: %s", qPrintable(errorString));
    }
    QVERIFY(!hasError);
    QVERIFY(dbRead.data());
    QCOMPARE(dbRead->rootGroup()->entries().size(), 1);
    Entry* entryRead = dbRead->rootGroup()->entries().at(0);
    EntryAttributes* attrRead = entryRead->attributes();

    QCOMPARE(strToBytes(attrRead->value("PlainInvalid")), QByteArray());
    QCOMPARE(strToBytes(attrRead->value("PlainValid")), strToBytes(strPlainValid));
    QCOMPARE(strToBytes(attrRead->value("SingleHighSurrogate1")), QByteArray());
    QCOMPARE(strToBytes(attrRead->value("SingleHighSurrogate2")), strToBytes(QString("12")));
    QCOMPARE(strToBytes(attrRead->value("HighHighSurrogate")), QByteArray());
    QCOMPARE(strToBytes(attrRead->value("SingleLowSurrogate1")), QByteArray());
    QCOMPARE(strToBytes(attrRead->value("SingleLowSurrogate2")), strToBytes(QString("12")));
    QCOMPARE(strToBytes(attrRead->value("LowLowSurrogate")), QByteArray());
    QCOMPARE(strToBytes(attrRead->value("SurrogateValid1")), strToBytes(strSurrogateValid1));
    QCOMPARE(strToBytes(attrRead->value("SurrogateValid2")), strToBytes(strSurrogateValid2));
}

void TestKeePass2Format::testXmlRepairUuidHistoryItem()
{
    QString xmlFile = QString("%1/%2.xml").arg(KEEPASSX_TEST_DATA_DIR, "BrokenDifferentEntryHistoryUuid");
    QVERIFY(QFile::exists(xmlFile));
    bool hasError;
    QString errorString;
    QScopedPointer<Database> db(readXml(xmlFile, false, hasError, errorString));
    if (hasError) {
        qWarning("Database read error: %s", qPrintable(errorString));
    }
    QVERIFY(!hasError);

    QList<Entry*> entries = db->rootGroup()->entries();
    QCOMPARE(entries.size(), 1);
    Entry* entry = entries.at(0);

    QList<Entry*> historyItems = entry->historyItems();
    QCOMPARE(historyItems.size(), 1);
    Entry* historyItem = historyItems.at(0);

    QVERIFY(!entry->uuid().isNull());
    QVERIFY(!historyItem->uuid().isNull());
    QCOMPARE(historyItem->uuid(), entry->uuid());
}

void TestKeePass2Format::testReadBackTargetDb()
{
    // read back previously constructed KDBX
    CompositeKey key;
    key.addKey(PasswordKey("test"));

    bool hasError;
    QString errorString;

    m_kdbxTargetBuffer.seek(0);
    readKdbx(&m_kdbxTargetBuffer, key, m_kdbxTargetDb, hasError, errorString);
    if (hasError) {
        QFAIL(qPrintable(QString("Error while reading database: ").append(errorString)));
    }
    QVERIFY(m_kdbxTargetDb.data());
}

void TestKeePass2Format::testKdbxBasic()
{
    QCOMPARE(m_kdbxTargetDb->metadata()->name(), m_kdbxSourceDb->metadata()->name());
    QVERIFY(m_kdbxTargetDb->rootGroup());
    QCOMPARE(m_kdbxTargetDb->rootGroup()->children()[0]->name(), m_kdbxSourceDb->rootGroup()->children()[0]->name());
    QCOMPARE(m_kdbxTargetDb->rootGroup()->notes(), m_kdbxSourceDb->rootGroup()->notes());
    QCOMPARE(m_kdbxTargetDb->rootGroup()->children()[0]->notes(), m_kdbxSourceDb->rootGroup()->children()[0]->notes());
}

void TestKeePass2Format::testKdbxProtectedAttributes()
{
    QCOMPARE(m_kdbxTargetDb->rootGroup()->entries().size(), 1);
    Entry* entry = m_kdbxTargetDb->rootGroup()->entries().at(0);
    QCOMPARE(entry->attributes()->value("test"), QString("protectedTest"));
    QCOMPARE(entry->attributes()->isProtected("test"), true);
}

void TestKeePass2Format::testKdbxAttachments()
{
    Entry* entry = m_kdbxTargetDb->rootGroup()->entries().at(0);
    QCOMPARE(entry->attachments()->keys().size(), 2);
    QCOMPARE(entry->attachments()->value("myattach.txt"), QByteArray("this is an attachment"));
    QCOMPARE(entry->attachments()->value("aaa.txt"), QByteArray("also an attachment"));
}

void TestKeePass2Format::testKdbxNonAsciiPasswords()
{
    QCOMPARE(m_kdbxTargetDb->rootGroup()->entries()[0]->password(), m_kdbxSourceDb->rootGroup()->entries()[0]->password());
}

void TestKeePass2Format::testKdbxDeviceFailure()
{
    CompositeKey key;
    key.addKey(PasswordKey("test"));
    QScopedPointer<Database> db(new Database());
    db->setKey(key);
    // Disable compression so we write a predictable number of bytes.
    db->setCompressionAlgo(Database::CompressionNone);

    auto entry = new Entry();
    entry->setParent(db->rootGroup());
    QByteArray attachment(4096, 'Z');
    entry->attachments()->set("test", attachment);

    FailDevice failDevice(512);
    QVERIFY(failDevice.open(QIODevice::WriteOnly));
    bool hasError;
    QString errorString;
    writeKdbx(&failDevice, db.data(), hasError, errorString);
    QVERIFY(hasError);
    QCOMPARE(errorString, QString("FAILDEVICE"));
}



// ====================================================================================================
// Helper functions
// ====================================================================================================

namespace QTest {
template<>
char* toString(const Uuid& uuid)
{
    QByteArray ba = "Uuid(";
    ba += uuid.toBase64().toLatin1().constData();
    ba += ")";
    return qstrdup(ba.constData());
}

template<>
char* toString(const Group::TriState& triState)
{
    QString value;

    if (triState == Group::Inherit) {
        value = "null";
    } else if (triState == Group::Enable) {
        value = "true";
    } else {
        value = "false";
    }

    return qstrdup(value.toLocal8Bit().constData());
}
}

QDateTime TestKeePass2Format::genDT(int year, int month, int day, int hour, int min, int second)
{
    QDate date(year, month, day);
    QTime time(hour, min, second);
    return QDateTime(date, time, Qt::UTC);
}

QByteArray TestKeePass2Format::strToBytes(const QString& str)
{
    QByteArray result;

    for (auto i : str) {
        result.append(static_cast<char>(i.unicode() >> 8));
        result.append(static_cast<char>(i.unicode() & 0xFF));
    }

    return result;
}
