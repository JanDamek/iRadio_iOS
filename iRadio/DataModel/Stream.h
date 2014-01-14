//
//  Stream.h
//  iRadioHD
//
//  Created by Jan Damek on 14.01.14.
//  Copyright (c) 2014 Jan Damek. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <CoreData/CoreData.h>

@class Radio;

@interface Stream : NSManagedObject

@property (nonatomic, retain) NSNumber * def3g;
@property (nonatomic, retain) NSString * url;
@property (nonatomic, retain) NSDate * timeStamp;
@property (nonatomic, retain) NSNumber * stream_id;
@property (nonatomic, retain) NSNumber * bitrate;
@property (nonatomic, retain) NSString * type;
@property (nonatomic, retain) NSString * name;
@property (nonatomic, retain) NSNumber * defWifi;
@property (nonatomic, retain) NSDate * updated;
@property (nonatomic, retain) NSNumber * user_def;
@property (nonatomic, retain) Radio *radio_rel;

@end
