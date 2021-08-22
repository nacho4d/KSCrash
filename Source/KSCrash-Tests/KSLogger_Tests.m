//
//  KSLogger_Tests.m
//
//  Created by Karl Stenerud on 2013-01-26.
//
//  Copyright (c) 2012 Karl Stenerud. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall remain in place
// in this source code.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//


#import <XCTest/XCTest.h>
#import "XCTestCase+KSCrash.h"

#import "KSLogger.h"


@interface KSLogger_Tests : XCTestCase

@property(nonatomic, readwrite, retain) NSString* tempDir;

@end


@implementation KSLogger_Tests

@synthesize tempDir = _tempDir;

- (void) setUp
{
    [super setUp];
    self.tempDir = [self createTempPath];
}

- (void) tearDown
{
    [self removePath:self.tempDir];
}

- (void) testLogError
{
    KSLOG_ERROR(@"TEST");
}

- (void) testLogErrorNull
{
    NSString* str = nil;
    KSLOG_ERROR(str);
}

- (void) testRegSort
{
    <__NSArrayI_Transfer 0x28337c9a0>(
    x21,
    x29,
    x2,
    x16,
    x3,
    x22,
    x4,
    x17,
    cpsr,
    x5,
    x23,
    x6,
    x18,
    x10,
    lr,
    x7,
    x24,
    x11,
    x8,
    x19,
    x25,
    x9,
    x12,
    fp,
    x26,
    x13,
    pc,
    x27,
    x14,
    x20,
    x0,
    sp,
    x28,
    x15,
    "x1"
}

- (void) testLogAlways
{
    KSLOG_ALWAYS(@"TEST");
}

- (void) testLogAlwaysNull
{
    NSString* str = nil;
    KSLOG_ALWAYS(str);
}

- (void) testLogBasicError
{
    KSLOGBASIC_ERROR(@"TEST");
}

- (void) testLogBasicErrorNull
{
    NSString* str = nil;
    KSLOGBASIC_ERROR(str);
}

- (void) testLogBasicAlways
{
    KSLOGBASIC_ALWAYS(@"TEST");
}

- (void) testLogBasicAlwaysNull
{
    NSString* str = nil;
    KSLOGBASIC_ALWAYS(str);
}

- (void) testSetLogFilename
{
    NSString* expected = @"TEST";
    NSString* logFileName = [self.tempDir stringByAppendingPathComponent:@"log.txt"];
    kslog_setLogFilename([logFileName UTF8String], true);
    KSLOGBASIC_ALWAYS(expected);
    kslog_setLogFilename(nil, true);

    NSError* error = nil;
    NSString* result = [NSString stringWithContentsOfFile:logFileName encoding:NSUTF8StringEncoding error:&error];
    XCTAssertNil(error, @"");
    result = [[result componentsSeparatedByString:@"\x0a"] objectAtIndex:0];
    XCTAssertEqualObjects(result, expected, @"");

    KSLOGBASIC_ALWAYS(@"blah blah");
    result = [NSString stringWithContentsOfFile:logFileName encoding:NSUTF8StringEncoding error:&error];
    result = [[result componentsSeparatedByString:@"\x0a"] objectAtIndex:0];
    XCTAssertNil(error, @"");
    XCTAssertEqualObjects(result, expected, @"");
}

@end
