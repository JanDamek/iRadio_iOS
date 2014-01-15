//
//  comXMLParseAbRadio.m
//  iRadioHD
//
//  Created by Jan Damek on 13.01.14.
//  Copyright (c) 2014 Jan Damek. All rights reserved.
//

#import "comXMLParseAbRadio.h"
#import "comData.h"
#import "comAppDelegate.h"
#import "XMLReader.h"
#import "comDamek.h"

@interface comXMLParseAbRadio (){
    BOOL _isInLoadXML;
}

@property (nonatomic, strong, getter = getData) comData *data;

@end

static NSString *kURLXMLData       = @"http://m.abradio.cz/xml/export.xml";

@implementation comXMLParseAbRadio

@synthesize data = _data;

-(id)init{
    self = [super init];
    
    _isInLoadXML = NO;
    
    return self;
}

-(comData *)getData{
    if (!_data){
        comAppDelegate *d = (comAppDelegate*)[[UIApplication sharedApplication] delegate];
        _data = d.data;
    }
    
    return _data;
}

-(void)parseAbRadioXML{
    if (!_isInLoadXML)
    {
        _isInLoadXML = YES;
        NSString *URL = kURLXMLData;
        
        NSString *agentString = @"iRadio/1.0 (iPhone/iPad) AppleWebKit/525.27.1 (KHTML, like Gecko) Version/3.2.1 Safari/525.27.1";
        NSMutableURLRequest *request = [NSMutableURLRequest requestWithURL:
                                        [NSURL URLWithString:URL]];
        [request setValue:agentString forHTTPHeaderField:@"User-Agent"];
        
        _isInLoadXML = YES;
        NSError *error = nil;
        NSURLResponse *response = nil;
        NSData *data = [NSURLConnection sendSynchronousRequest:request returningResponse:&response error:&error];
        if (!error && [data length] > 0)
        {
            NSDictionary *d = [XMLReader dictionaryForXMLData:data error:nil];
            NSArray *cat = [[d objectForKey:@"categories"] objectForKey:@"category"];
            for (NSDictionary *item in cat) {
                //loop for all items in d
                
                int catId = [comDamek getInt:[item objectForKey:@"category_id"]];
                NSString *title = [comDamek getString:[item objectForKey:@"title"]];
                
                Categorie *cat = [self.data findCategorieId:catId];
                cat.category_id = [NSNumber numberWithInt:catId];
                cat.user_def = [NSNumber numberWithBool:NO];
                cat.title = title;
                
                //insert radios data
                NSArray *radios = [[item objectForKey:@"radios"] objectForKey:@"radio"];
                if ([radios isKindOfClass:[NSDictionary class]]){
                    [self doRadio:(NSDictionary*)radios :cat];
                }else
                    for (NSDictionary *r in radios) {
                        [self doRadio:r :cat];
                    }
            }
            [self.data saveCategories];
            [self.data saveRadios];            
            [self.data saveStreams];

            self.data.categories.fetchRequest.predicate = nil;
            self.data.radios.fetchRequest.predicate = nil;
            self.data.streams.fetchRequest.predicate = nil;
        }
        else
        {
            NSLog(@"error loading XML");
        }
    }
}

-(void)doRadio:(NSDictionary*)r :(Categorie*)cat{
    int radId = [comDamek getInt:[r objectForKey:@"id"]];
    
    Radio *rad = [self.data findRadioId:radId];
    
    rad.radio_id = [NSNumber numberWithInt:radId];
    rad.user_def = [NSNumber numberWithBool:NO];
    rad.descript = [comDamek getString:[r objectForKey:@"description"]];
    rad.name = [comDamek getString:[r objectForKey:@"name"]];
    //                         rad.updated =
    rad.interpret = [comDamek getString: [r objectForKey:@"interpret"]];
    rad.logo = [comDamek getString:[r objectForKey:@"logo"]];
    rad.artwork = [comDamek getString:[r objectForKey:@"artwork"]];
    rad.logo_ipad = [comDamek getString: [r objectForKey:@"logo_ipad"]];
    rad.program = [comDamek getString: [r objectForKey:@"program"]];
    
    [cat addRadios_relObject:rad];
    
    //insert stream data
    NSArray *streams = [[r objectForKey:@"streams"] objectForKey:@"stream"];
    if ([streams isKindOfClass:[NSDictionary class]]){
        [self doStream:(NSDictionary*)streams :rad];
    }else
        for (NSDictionary *s in streams) {
            [self doStream:s :rad];
        }
}

-(void)doStream:(NSDictionary*)s :(Radio*)rad{
    int strId = [comDamek getInt:[s objectForKey:@"id"]];
    
    Stream *str = [self.data findStreamId:strId];
    str.stream_id = [NSNumber numberWithInt:strId];
    str.user_def = [NSNumber numberWithBool:NO];
    str.def3g   = [comDamek getNumber:[s objectForKey:@"def3g"]];
    str.defWifi = [comDamek getNumber:[s objectForKey:@"defWifi"]];
    str.bitrate = [comDamek getNumber:[s objectForKey:@"bitrate"]];
    str.url = [comDamek getString:[s objectForKey:@"url"]];
    str.type = [comDamek getString:[s objectForKey:@"type"]];
    str.name = [comDamek getString:[s objectForKey:@"name"]];
    // str.updated;
    
    [rad addStream_relObject:str];
}
@end
