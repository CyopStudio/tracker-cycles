#ifndef LIBS_RADIO_SI4703_REGDEFS_H_
#define LIBS_RADIO_SI4703_REGDEFS_H_

#define rDEVICEID 0x00
#define MFGID(X)  (X & 0x0FFF)
#define PN(X)     ((X & 0x0F) << 12)

#define rCHIPID     0x01
#define FIRMWARE(X) (X & 0x1F)
#define DEV(X)      ((X & 0x0F) << 6)
#define REV(X)      ((X & 0x3F) << 11)

#define rPOWERCFG 0x02
#define ENABLE    0x01
#define DISABLE   0x40
#define SEEK      0x0100
#define SEEKUP    0x0200
#define SKMODE    0x0400
#define RDSM      0x0800
#define MONO      0x2000
#define DMUTE     0x4000
#define DSMUTE    0x8000

#define rCHANNEL   0x03
#define CHANN_MASK 0x03FF
#define CHANN(X)   (X & CHANN_MASK)
#define TUNE       0x8000U

#define rSYSCONFIG1 0x04
#define GPIO1(X)    (X & 0x03)
#define GPIO2(X)    ((X & 0x03) << 2)
#define GPIO3(X)    ((X & 0x03) << 4)
#define BLINDADJ(X) ((X & 0x03) << 6)
#define AGCD        0x0400
#define DE          0x0800
#define RDS         0x1000
#define STCIEN      0x4000
#define RDSIEN      0x8000

#define rSYSCONFIG2 0x05
#define VOLUME_MASK 0x0F
#define VOLUME(X)   (X & VOLUME_MASK)
#define SPACE(X)    ((X & 0x03) << 4)
#define BAND(X)     ((X & 0x03) << 6)
#define SEEKTH(X)   ((X & 0xFF) << 8)

#define rSYSCONFIG3 0x06
#define SKCNT(X)    (X & 0x0F)
#define SKSNR(X)    ((X & 0x0F) << 4)
#define VOLEXT      0x0100
#define SMUTEA(X)   ((X & 0x03) << 12)
#define SMUTER(X)   ((X & 0x03) << 14)

#define rTEST1 0x07
#define XOSCEN 0x8000
#define AHIZEN 0x4000

#define rTEST2 0x08

#define rBOOTCONFIG 0x09

#define rSTATUSRSSI 0x0A
#define RSSI(X)     (X & 0xFF)
#define ST          0x0100
#define BLERA(X)    ((X & 0x03) << 9)
#define RDSS        0x0800
#define AFCRL       0x1000
#define SF_BL       0x2000
#define STC         0x4000
#define RDSR        0x8000

#define rREADCHAN    0x0B
#define READCHANN(X) (X & 0x03FF)
#define BLERD(X)     ((X & 0x03) << 10)
#define BLERC(X)     ((X & 0x03) << 12)
#define BLERB(X)     ((X & 0x03) << 14)

#define rRDSA 0x0C

#define rRDSB 0x0D

#define rRDSC 0x0E

#define rRDSD 0x0F

#endif
