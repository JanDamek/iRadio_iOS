//
//  Stream.h
//  iRadio
//
//  Created by Jan Damek on 12.01.14.
//  Copyright (c) 2014 Jan Damek. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <CoreData/CoreData.h>

@class Radio;

@interface Stream : NSManagedObject

@property (nonatomic, retain) NSNumber * def3g;
@property (nonatomic, retain) NSString * url;
@property (nonatomic, retain) NSDate * timeStamp;
@property (nonatomic, retain) Radio *radio_rel;

@end
