#ifndef PSARCFILE_H
#define PSARCFILE_H

#include <QString>
#include <QStringList>
#include <QByteArray>
#include <QFile>
#include <QVector>
#include <QMap>
#include <QDataStream>
#include <memory>
#include <cstdint>

class PsarcFile
{
public:
    struct FileEntry {
        QString name;
        uint64_t offset = 0;
        uint64_t uncompressedSize = 0;
        uint32_t startChunkIndex = 0;
    };

    explicit PsarcFile(const QString& filePath);
    ~PsarcFile();

    bool open();
    void close();
    bool isOpen() const;

    QStringList getFileList() const;
    bool fileExists(const QString& fileName) const;
    QByteArray extractFile(const QString& fileName);
    bool extractFileTo(const QString& fileName, const QString& outputPath);
    bool extractAll(const QString& outputDirectory);

    QString getLastError() const { return m_lastError; }
    int getFileCount() const { return m_entries.size(); }
    const FileEntry* getEntry(int index) const;
    const FileEntry* getEntry(const QString& fileName) const;

private:
    struct Header {
        uint32_t magic = 0;
        uint16_t versionMajor = 0;
        uint16_t versionMinor = 0;
        char compressionMethod[4] = {0};
        uint32_t tocLength = 0;
        uint32_t tocEntrySize = 0;
        uint32_t numFiles = 0;
        uint32_t blockSize = 0;
        uint32_t archiveFlags = 0;
    };

    bool readHeader();
    bool readTOC();
    bool readManifest();

    QByteArray decryptTOC(const QByteArray& encryptedData);
    QByteArray decryptSNG(const QByteArray& encryptedData);

    QByteArray decompressZlib(const QByteArray& data, uint64_t uncompressedSize, bool& success);
    QByteArray decompressLZMA(const QByteArray& data, uint64_t uncompressedSize, bool& success);

    QByteArray extractFileByIndex(int entryIndex);

    QString m_filePath;
    std::unique_ptr<QFile> m_file;
    Header m_header;
    QVector<FileEntry> m_entries;
    QVector<uint16_t> m_zLengths;
    QMap<QString, int> m_fileMap;
    QString m_lastError;
    bool m_isOpen;
    bool m_tocEncrypted = false;
};

#endif // PSARCFILE_H
